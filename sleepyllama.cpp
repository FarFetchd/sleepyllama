#include <chrono>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <csignal>
#include <memory>
#include <thread>
using namespace std;

const int kActivityCheckPeriodSecs = 10;
const int kSleepTimeoutSecs = 60 * 60;

uint64_t curTimeMSSE()
{
  return chrono::duration_cast<chrono::milliseconds>(
      chrono::system_clock::now().time_since_epoch()).count();
}

string runShellSync(const char* cmd)
{
  char buffer[1024];
  string ret;
  unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);

  while (fgets(buffer, 1024, pipe.get()) != nullptr)
    ret += buffer;
  return ret;
}

int g_secs_since_busy = 0;
void handleSIGUSR1(int signal)
{
  g_secs_since_busy = kSleepTimeoutSecs + 1; // "sleep now"
}

int main(int argc, char** argv)
{
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s /path/to/llama_cpp_server_runner_script_path.sh\n",
            argv[0]);
    return 1;
  }
  signal(SIGUSR1, handleSIGUSR1);

  FILE* server_pipe = popen(argv[1], "r");
  while (true)
  {
    this_thread::sleep_for(chrono::seconds(kActivityCheckPeriodSecs));
    if (!runShellSync("nvidia-smi --query-gpu=pstate --format=csv,noheader | sed '/P8/d'").empty())
      g_secs_since_busy = 0;
    else
      g_secs_since_busy += kActivityCheckPeriodSecs;

    if (g_secs_since_busy > kSleepTimeoutSecs)
    {
      g_secs_since_busy = 0;
      uint64_t cur_time = curTimeMSSE();
      system("killall server");
      pclose(server_pipe);
      system("sudo systemctl suspend");
      uint64_t new_time = curTimeMSSE();
      while (new_time < cur_time + 3000) // detect time skip: resuming from sleep
      {
        cur_time = new_time;
        this_thread::sleep_for(chrono::milliseconds(1));
        new_time = curTimeMSSE();
      }
      server_pipe = popen(argv[1], "r");
    }
  }
  return 0;
}

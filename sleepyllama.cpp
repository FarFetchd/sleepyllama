#include <chrono>
#include <string>
#include <cstdlib>
#include <cstdio>
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

int main(int argc, char** argv)
{
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s /path/to/llama_cpp_server_runner_script_path.sh\n",
            argv[0]);
    return 1;
  }

  int secs_since_busy = 0;
  FILE* server_pipe = popen(argv[1], "r");
  while (true)
  {
    this_thread::sleep_for(chrono::seconds(kActivityCheckPeriodSecs));
    if (runShellSync("nvidia-smi | grep MiB | sed '/server/d' | grep P8").empty())
      secs_since_busy = 0;
    else
      secs_since_busy += kActivityCheckPeriodSecs;

    if (secs_since_busy > kSleepTimeoutSecs)
    {
      secs_since_busy = 0;
      uint64_t cur_time = curTimeMSSE();
      system("killall server");
      pclose(server_pipe);
      system("sudo systemctl suspend");
      uint64_t new_time = curTimeMSSE();
      while (new_time < cur_time + 5000) // detect time skip: resuming from sleep
      {
        cur_time = new_time;
        this_thread::sleep_for(chrono::milliseconds(100));
        new_time = curTimeMSSE();
      }
      server_pipe = popen(argv[1], "r");
    }
  }
  return 0;
}

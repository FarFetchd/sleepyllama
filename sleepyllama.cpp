#include <chrono>
#include <string>
#include <cstdlib>
#include <cstdio>
#include <csignal>
#include <memory>
#include <thread>
using namespace std;

const int kSleepTimeoutSecs = 1 * 60 * 60; // sleep after 1 hour idling in pstate 8
const char kInferenceShutdownCmd[] = "killall llama-server whisper-server";
const char kSystemSleepCmd[] = "sudo systemctl suspend";

const int kActivityCheckPeriodSecs = 10;

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

void runAll(FILE** server_pipes, int argc, char** argv)
{
  for (int i=0; i<argc-1; i++)
  {
    fprintf(stderr, "now running %s\n", argv[i+1]);
    server_pipes[i] = popen(argv[i+1], "r");
  }
}
void closeAllPipes(FILE** server_pipes, int argc)
{
  for (int i=0; i<argc-1; i++)
    pclose(server_pipes[i]);
}

int main(int argc, char** argv)
{
  if (argc < 2)
  {
    fprintf(stderr, "usage: %s /path/to/llama_cpp_server_runner_script_path1.sh script_path2.sh etc\n"
                    "(will keep all servers started by those scripts running)\n"
                    "(be sure all server binaries used are listed in kInferenceShutdownCmd)",
            argv[0]);
    return 1;
  }
  signal(SIGUSR1, handleSIGUSR1);

  FILE** server_pipes = (FILE**)malloc(sizeof(FILE*) * (argc-1));
  runAll(server_pipes, argc, argv);
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
      system(kInferenceShutdownCmd);
      closeAllPipes(server_pipes, argc); // blocks until all server children exit
      system(kSystemSleepCmd);
      uint64_t new_time = curTimeMSSE();
      while (new_time < cur_time + 3000) // detect time skip: resuming from sleep
      {
        cur_time = new_time;
        this_thread::sleep_for(chrono::milliseconds(1));
        new_time = curTimeMSSE();
      }
      runAll(server_pipes, argc, argv);
    }
  }
  return 0;
}

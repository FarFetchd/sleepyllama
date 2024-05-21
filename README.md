![sleepy llama](https://raw.githubusercontent.com/FarFetchd/sleepyllama/bc6d9d81686640683b912fcabc1d8dc51d0ed33c/sleepyllama.jpg)

# sleepyllama

An auto-sleeping and -waking framework around llama.cpp: your inference server will go to sleep after sitting idle for a certain length of time, and wake back up when you start interacting with your frontend.

# Architecture

sleepyllama has two parts: a reverse proxy, and a wrapper around llama.cpp. The reverse proxy needs to run on an always-on machine. Whenever it finds itself unable to talk to the specified backend IP:port, it holds the client's connection open and starts sending wake-on-LAN packets, until it can complete the connection to the backend. The llama.cpp wrapper is necessary both to sleep on idle, and to kill and restart the llama.cpp server, since CUDA GPUs don't handle sleep well.

# Usage

Compile on your inference machine with `g++ -o sleepyllama sleepyllama.cpp`. Write the entire command line you use to run the llama.cpp server into an executable script file, then run the wrapper as `./sleepyllama that_script_file.sh`.

Fill in the IP/port/etc `#define`s at the top of reverse_proxy_WoL.cc, then compile it on your always-on machine with `g++ -o reverse_proxy_WoL reverse_proxy_WoL.cpp -lpthread`. It takes no arguments.

Apply llama_cpp_server_pstates.patch to llama.cpp's examples/server/server.cpp before compiling it. It uses nvidia-pstate, which is a python package: `pip3 install nvidia_pstate`. This allows sleepyllama to watch for idling, and also saves 10s of watts per GPU while idle. Do this even if you decide not to use sleepyllama. Idea from [ToriLinux](https://github.com/sasha0552/ToriLinux)

Finally, where you previously pointed your frontend at the llama.cpp server, now point it at the reverse proxy. You're all set!

# Notes

reverse_proxy_WoL needs to be able to run `sudo etherwake`, and to live on the same network as the backend. (This project is very much supposed to be a couple of physical machines that live in your home). The inference machine needs wake-on-LAN enabled in BIOS. sleepyllama needs to be able to run `nvidia-smi` and `sudo systemctl suspend`.

The go-to-sleep logic looks only at GPU activity (specifically whether it's idling in pstate 8), so you'd need to adjust that if you want to use the machine for anything other than inference (unless you're ok with getting booted every hour!)

You could easily adapt this system to work with other inference backends, or any kind of GPU-using server, really. Just change the `killall server` in sleepyllama.cpp to your server's name, and make sure your server sets pstate 8 when idle ([see other patch examples](https://github.com/sasha0552/ToriLinux/tree/main/airootfs/home/tori/.local/share/tori/patches)).

#!/bin/bash

while [[ -n $(ps aux | grep server | grep llama) ]] ; do
  killall llama-server sleepyllama
  sleep 1
done
nvidia-pstate -s -ps 16

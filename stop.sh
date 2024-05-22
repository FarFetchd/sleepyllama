#!/bin/bash

while [[ -n $(ps aux | grep server | grep llama) ]] ; do
  killall server sleepyllama
  sleep 1
done

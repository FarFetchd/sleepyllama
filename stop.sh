#!/bin/bash

while [[ -n $(pgrep server) ]] ; do
  killall server sleepyllama
  sleep 1
done

#!/bin/bash

# Triggers sleepyllama's sleep, just as if the idle timeout had elapsed.
killall -SIGUSR1 sleepyllama

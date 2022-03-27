#!/bin/sh

cd /home/pi/Desktop/AudioLatencyMeasurement/
make
sudo ./audio_lag_module
$SHELL
#!/bin/sh

sudo cp ./udev/11-media-by-label-auto-mount.rules /etc/udev/rules.d/
sudo cp ./udev/85-my-usb-audio.rules /etc/udev/rules.d/
#sudo cp ./start_audio_lag_module.sh /etc/profile.d/
sudo chmod +x start_audio_lag_module.sh
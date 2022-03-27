#!/bin/sh

sudo cp ./11-media-by-label-auto-mount.rules /etc/udev/rules.d/
sudo cp ./85-my-usb-audio.rules /etc/udev/rules.d/
chmod +x start_audio_lag_module.sh
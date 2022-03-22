all:
	gcc -Wall -pthread audio_lag_module.c -lasound -o audio_lag_module -lpigpio -lrt

all:
	gcc -Wall -pthread -o latency_measurement latency_measurement.c -lpigpio -lrt
	gcc -Wall -pthread alsa_out.c -lasound -o alsa_out -lpigpio -lrt

all:
	gcc -Wall -pthread -o latency_measurement latency_measurement.c -lpigpio -lrt
	gcc -Wall -pthread -o alsa_out alsa_out.c -lpigpio -lrt
	gcc alsa_out.c -lasound -o alsa_out

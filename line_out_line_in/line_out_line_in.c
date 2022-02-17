#include <pigpio.h>
#include <stdio.h>

#define LINE_OUT 17
#define LINE_IN 27
#define TOTAL_MEASUREMENTS 10
#define SIGNAL_LENGTH_IN_S 0.001
#define SIGNAL_START_INTERVAL_IN_S 1.0
#define SIGNAL_ON_THE_WAY 0
#define SIGNAL_ARRIVED 1

uint32_t startTimestamp, endTimestamp;
int latencyInMicros;
int latencyMeasurementsInMicros[TOTAL_MEASUREMENTS];
int validMeasurmentsCount = 0;
int maxLatencyInMicros = -1;
int minLatencyInMicros = -1;
int sumOfLatenciesInMicros = 0;
int avgLatencyInMicros;
int signalStatus;
int gpioStatus;

// Line-out callback
void onLineOut(int gpio, int level, uint32_t tick) {
    printf("GPIO %d state changed to level %d at %d\n", gpio, level, tick);
    
    // Rising Edge
    if (level == 1) {
        startTimestamp = tick;
        signalStatus = SIGNAL_ON_THE_WAY;
    }
}

// Line-in callback
void onLineIn(int gpio, int level, uint32_t tick) {
    printf("GPIO %d state changed to level %d at %d\n", gpio, level, tick);
    
    // Rising Edge
    if (level == 1) {

        // This condition avoids, that multiple trigger of the transistor lead to reassignment of the endTimestamp
        if (signalStatus == SIGNAL_ON_THE_WAY) {
            endTimestamp = tick;
            signalStatus = SIGNAL_ARRIVED;

            latencyInMicros = endTick - startTick;

            // The uint32_t tick parameter represents the number of microseconds since boot.
            // This wraps around from 4294967295 to 0 approximately every 72 minutes.
            // Thats why the provided latency could be both negative and wrong in this specific situation.
            if (latencyInMicros >= 0) {
                
                // Saving valid measurement
                latencyMeasurementsInMicros[validMeasurmentsCount] = latencyInMicros;
                printf("The signal had a latency of %d microseconds\n", latencyInMicros);
                validMeasurmentsCount += 1;
                
                // Calculating running descriptive values (min, max, avg)
                // max
                if (maxLatencyInMicros == -1) {
                    maxLatencyInMicros = latencyInMicros;
                }
                else if (latencyInMicros > maxLatencyInMicros) {
                    maxLatencyInMicros = latencyInMicros;
                }
                else {}
                // min
                if (minLatencyInMicros == -1) {
                    minLatencyInMicros = latencyInMicros;
                }
                else if (latencyInMicros < minLatencyInMicros) {
                    minLatencyInMicros = latencyInMicros;
                }
                else {}
                // avg
                sumOfLatenciesInMicros += latencyInMicros;
                avgLatencyInMicros = sumOfLatenciesInMicros / validMeasurmentsCount;
            }
        }  
    }
}

int main(void) {
    double signalIntervalInS, maxLatencyInS;

    // Initialize library
    gpioStatus = gpioInitialise();
    //printf("Status after gpioInitialise: %d\n", gpioStatus);

    // Set GPIO Modes
    gpioSetMode(LINE_OUT, PI_OUTPUT);
    gpioSetMode(LINE_IN, PI_INPUT);

    // Register GPIO state change callback
    gpioSetAlertFunc(LINE_OUT, onLineOut);
    gpioSetAlertFunc(LINE_IN, onLineIn);

    // Fill measurement array with -1 values to mark invalid measurements
    for (int i = 0; i < TOTAL_MEASUREMENTS; i++) {
        latencyMeasurementsInMicros[i] = -1;
    }

    // Send 3.3V squarewave signals through the line output. They have a length of SIGNAL_LENGTH_IN_S
    // The interval from the first to the second signal is SIGNAL_START_INTERVAL_IN_S
    signalIntervalInS = SIGNAL_START_INTERVAL_IN_S;
    for (int i = 0; i < TOTAL_MEASUREMENTS; i++) {

        // After the first signal, the signal interval converges to the maximum measured latency
        if (i > 0) {
            maxLatencyInS = (double) maxLatencyInMicros / 1000000.0;
            signalIntervalInS = maxLatencyInS + 1 / i * maxLatencyInS;
        }
        
        // Send signal through LINE_OUT gpio pin
        printf("\n\n----- Measurement %d started -----\n", i + 1);
        gpioStatus = gpioWrite(LINE_OUT, 1);
        //printf("status (0 = OK; <0 = ERROR): %d\n", gpioStatus);
        time_sleep(SIGNAL_LENGTH_IN_S);
        gpioStatus = gpioWrite(LINE_OUT, 0);
        //printf("status (0 = OK; <0 = ERROR): %d\n", gpioStatus);
        time_sleep(signalIntervalInS);
    }

    // TODO: Remove status variable or handle errors
    printf("\n%d\n", gpioStatus);
    
    // Print measurements
    for (int i = 0; i < TOTAL_MEASUREMENTS; i++) {
        printf("\n##### Measurement %d latency: %d\n", i + 1, latencyMeasurementsInMicros[i]);
    }
    
    // Terminate library
    gpioTerminate();
    
    // TODO: Saving measurements to .csv format
    
    printf("\nExit\n");
}

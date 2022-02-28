#include <pigpio.h>
#include <stdio.h>

#define TOTAL_MEASUREMENTS 10

#define LINE_OUT 17 // GPIO 17
#define LINE_IN 27 // GPIO 27
#define SIGNAL_LENGTH_IN_S 0.001
#define SIGNAL_START_INTERVAL_IN_S 1.0
#define SIGNAL_MINIMUM_INTERVAL_IN_S 0.02 // Minimum interval to ensure correct amplification
#define SIGNAL_ARRIVED 1
#define SIGNAL_ON_THE_WAY 0
#define DISPLAY_AVERAGE 1
#define DISPLAY_MAXIMUM 2
#define DISPLAY_MINIMUM 3

// User inputs
#define START_MEASUREMENT 1 // TODO: GPIO numbers
#define START_CALIBRATION 2
#define LINE_LEVEL_MODE 3
#define PCIE_MODE 4
#define USB_MODE 5
#define HDMI_MODE 6
#define CHANGE_DISPLAY 7

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
int measurementMode = LINE_LEVEL_MODE;
int displayModes[] = {DISPLAY_AVERAGE, DISPLAY_MAXIMUM, DISPLAY_MINIMUM};
int displayMode = DISPLAY_AVERAGE;
int displayModeCount = 0;


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

            latencyInMicros = endTimestamp - startTimestamp;

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

                // TODO: displayDescriptiveValues(displayMode, avg, max, min)
            }
        }  
    }
}

// Line-out signal creation
void sendSignalViaLineOut(double signalLengthInS, double signalIntervalInS) {

    // Send signal through LINE_OUT gpio pin
    gpioStatus = gpioWrite(LINE_OUT, 1);
    //printf("status (0 = OK; <0 = ERROR): %d\n", gpioStatus);
    time_sleep(signalLengthInS);
    gpioStatus = gpioWrite(LINE_OUT, 0);
    //printf("status (0 = OK; <0 = ERROR): %d\n", gpioStatus);
    time_sleep(signalIntervalInS);
}

void startMeasurement(int measurementMode) {
    double signalIntervalInS, maxLatencyInS;

    // The interval from the first to the second signal is SIGNAL_START_INTERVAL_IN_S
    signalIntervalInS = SIGNAL_START_INTERVAL_IN_S;
    for (int i = 0; i < TOTAL_MEASUREMENTS; i++) {

        // After the first signal that arrived, the signal interval converges to the maximum measured latency
        // If its smaller than SIGNAL_MINIMUM_INTERVAL_IN_S it converges to that value
        if (maxLatencyInMicros != -1 && i > 0) {
            maxLatencyInS = (double) maxLatencyInMicros / 1000000.0;
            if (maxLatencyInS <= SIGNAL_MINIMUM_INTERVAL_IN_S) {
                signalIntervalInS = SIGNAL_MINIMUM_INTERVAL_IN_S + 1 / i * maxLatencyInS;
            }
            else {
                signalIntervalInS = maxLatencyInS + 1 / i * maxLatencyInS;
            }
        }

        // Send 3.3V squarewave signals through the line output with specified length and interval
        if (measurementMode == LINE_LEVEL_MODE) {
            printf("\n\n----- Measurement %d started -----\n", i + 1);
            sendSignalViaLineOut(SIGNAL_LENGTH_IN_S, signalIntervalInS);
        }
        // TODO: else if (measurementMode == USB, PCIe...
    }
    // TODO: Saving measurements to .csv format
}

// TODO
void startCalibration() {
    //
}

void onUserInput(int gpio, int level, uint32_t tick) {

    if (level == 1) {
        if (gpio == START_MEASUREMENT) {
            startMeasurement(measurementMode);
        }
        else if (gpio == START_CALIBRATION) {
            startCalibration();
        }
        else if (gpio == CHANGE_DISPLAY) {
            if (displayModeCount == sizeof(displayModes) / sizeof(displayModes[0]) - 1) {
                displayModeCount = 0;
            }
            else {
                displayModeCount += 1;
            }
            displayMode = displayModes[displayModeCount];
        }
        // TODO: measurementMode changes
        else {
            
        }
    }
}

void initGpioLibrary() {

    // Initialize library
    gpioStatus = gpioInitialise();
    //printf("Status after gpioInitialise: %d\n", gpioStatus);

    // Set GPIO Modes
    gpioSetMode(LINE_OUT, PI_OUTPUT);
    gpioSetMode(LINE_IN, PI_INPUT);

    // Register GPIO state change callback
    gpioSetAlertFunc(LINE_OUT, onLineOut);
    gpioSetAlertFunc(LINE_IN, onLineIn);
}

/*void initALSA() {
    for (int i = 0; i < sizeof(buffer); i++) {
        buffer[i] = 0xff;
    }
}*/

/*void waitForUserInput() {
    while (1) {
        // Waiting for input gpio callbacks in onUserInput()
    }
}*/

int main(void) {

    // TODO: init gpio callbacks for the measurementMode LINE_LEVEL, USB, PCIE...
    initGpioLibrary();

    //initALSA();

    // Fill measurement array with -1 values to mark invalid measurements
    for (int i = 0; i < TOTAL_MEASUREMENTS; i++) {
        latencyMeasurementsInMicros[i] = -1;
    }

    // waitForUserInput();
    startMeasurement(LINE_LEVEL_MODE);

    // TODO: Remove status variable or handle errors
    printf("\n%d\n", gpioStatus);
    
    // Print measurements
    for (int i = 0; i < TOTAL_MEASUREMENTS; i++) {
        printf("\n##### Measurement %d latency: %d\n", i + 1, latencyMeasurementsInMicros[i]);
    }
    
    // Terminate library
    gpioTerminate();
    
    printf("\nExit\n");
}

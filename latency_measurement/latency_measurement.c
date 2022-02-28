/*
ALSA playback code base retrieved from https://www.alsa-project.org/alsa-doc/alsa-lib/_2test_2pcm__min_8c-example.html on 28th February 2022
ALSA hardware parameter code base retrieved from https://www.linuxjournal.com/article/6735 on 28th February 2022
*/

#include <pigpio.h>
#include <alsa/asoundlib.h>
#include <stdio.h>

#define TOTAL_MEASUREMENTS 10
// BUFFER_SIZE = ALSA_PCM_PREFERRED_SAMPLE_RATE (48000 kHz) * SIGNAL_LENGTH_IN_S (0.001 s)
#define BUFFER_SIZE 480

const int LINE_IN = 27; // GPIO 27
const int LINE_OUT = 17; // GPIO 17
const int ALSA_PCM_SOFT_RESAMPLE = 0;
const unsigned int ALSA_PCM_LATENCY = 0;
const unsigned int ALSA_PCM_PREFERRED_SAMPLE_RATE = 48000;
const double SIGNAL_LENGTH_IN_S = 0.001;
const double SIGNAL_START_INTERVAL_IN_S = 1.0;
const double SIGNAL_MINIMUM_INTERVAL_IN_S = 0.02; // Minimum interval to ensure correct amplification
const int SIGNAL_ARRIVED = 1;
const int SIGNAL_ON_THE_WAY = 0;
const int DISPLAY_AVERAGE = 1;
const int DISPLAY_MAXIMUM = 2;
const int DISPLAY_MINIMUM = 3;

// User inputs
const int START_MEASUREMENT = 1; // TODO: GPIO numbers
const int START_CALIBRATION = 2;
const int LINE_LEVEL_MODE = 3;
const int ALSA_PCIE_MODE = 4;
const int ALSA_USB_MODE = 5;
const int ALSA_HDMI_MODE = 6;
const int CHANGE_DISPLAY = 7;

// ALSA variables

char *alsaUSBOut = "hw:1,0";
char *alsaHDMI1Out = "hw:2,0";
char *alsaHDMI0Out = "hw:0,0";
char *alsaPcmDevice = alsaUSBOut;
snd_pcm_format_t formatType;
snd_pcm_access_t accessType;
unsigned int channels;
unsigned int sampleRate = ALSA_PCM_PREFERRED_SAMPLE_RATE;
unsigned char buffer[BUFFER_SIZE];

// Latency measurement
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
void sendSignalViaLineOut(double signalIntervalInS) {

    // Send signal through LINE_OUT gpio pin
    gpioStatus = gpioWrite(LINE_OUT, 1);
    //printf("status (0 = OK; <0 = ERROR): %d\n", gpioStatus);
    time_sleep(SIGNAL_LENGTH_IN_S);
    gpioStatus = gpioWrite(LINE_OUT, 0);
    //printf("status (0 = OK; <0 = ERROR): %d\n", gpioStatus);
    time_sleep(signalIntervalInS);
}

/* Get information about the PCM interface */
void getPCMHardwareParameters() {
    int rc;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    unsigned int val;
    int dir;

    /* Open PCM device for playback. */
    rc = snd_pcm_open(&handle, alsaPcmDevice,
            SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) {
        fprintf(stderr,
                "unable to open pcm device: %s\n",
                snd_strerror(rc));
        exit(1);
    }

    /* Allocate a hardware parameters object. */
    snd_pcm_hw_params_alloca(&params);

    /* Fill it in with default values. */
    snd_pcm_hw_params_any(handle, params);

    /* Set the desired hardware parameters. */

    /* Interleaved mode */
    snd_pcm_hw_params_set_access(handle, params,
            SND_PCM_ACCESS_RW_INTERLEAVED);

    /* Signed 16-bit little-endian format */
    snd_pcm_hw_params_set_format(handle, params,
            SND_PCM_FORMAT_S16_LE);

    /* Two channels (stereo) */
    snd_pcm_hw_params_set_channels(handle, params, channels);

    /* 48000 bits/second sampling rate */
    snd_pcm_hw_params_set_rate_near(handle, params, &sampleRate, &dir);

    /* Write the parameters to the driver */
    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0) {
        fprintf(stderr,
                "unable to set hw parameters: %s\n",
                snd_strerror(rc));
        exit(1);
    }

    /* Get information about the PCM interface */

    snd_pcm_hw_params_get_access(params, (snd_pcm_access_t *) &val);
    accessType = (snd_pcm_access_t) val;

    snd_pcm_hw_params_get_format(params, (snd_pcm_format_t *) &val);
    formatType = (snd_pcm_format_t) val;

    snd_pcm_hw_params_get_channels(params, &val);
    channels = val;

    snd_pcm_hw_params_get_rate(params, &val, &dir);
    sampleRate = val;

    snd_pcm_close(handle);
}

void sendSignalViaALSA(double signalIntervalInS) {
    int err;
    snd_pcm_t *handle;

    if ((err = snd_pcm_open(&handle, alsaPcmDevice, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
            printf("Playback open error: %s\n", snd_strerror(err));
            exit(EXIT_FAILURE);
    }


    if ((err = snd_pcm_set_params(handle,
                                    formatType,
                                    accessType,
                                    channels,
                                    sampleRate,
                                    ALSA_PCM_SOFT_RESAMPLE,
                                    ALSA_PCM_LATENCY)) < 0) {
            printf("Playback open error: %s\n", snd_strerror(err));
            exit(EXIT_FAILURE);
    }

    signalStatus = SIGNAL_ON_THE_WAY;
    // Send signal through alsa pcm device
    snd_pcm_writei(handle, buffer, sizeof(buffer));
    // Start measurement
    startTimestamp = gpioTick();

    snd_pcm_close(handle);
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
            sendSignalViaLineOut(signalIntervalInS);
        }
        else {
            sendSignalViaALSA(signalIntervalInS);
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
        // Measurement mode got changed
        else {
            measurementMode = gpio;
            if (gpio == ALSA_USB_MODE) {
                alsaPcmDevice = alsaUSBOut;
            }
            else if (gpio == ALSA_HDMI_MODE) {
                alsaPcmDevice = alsaHDMI1Out;
            }
            // TODO: measurementMode changes
            else {}
            getPCMHardwareParameters();
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

void initALSA() {
    for (int i = 0; i < sizeof(buffer); i++) {
        buffer[i] = 0xff;
    }
    getPCMHardwareParameters();
}

/*void waitForUserInput() {
    while (1) {
        // Waiting for input gpio callbacks in onUserInput()
    }
}*/

int main(void) {

    // TODO: init gpio callbacks for the measurementMode LINE_LEVEL, USB, PCIE...
    initGpioLibrary();

    initALSA();

    // Fill measurement array with -1 values to mark invalid measurements
    for (int i = 0; i < TOTAL_MEASUREMENTS; i++) {
        latencyMeasurementsInMicros[i] = -1;
    }

    // waitForUserInput();
    startMeasurement(ALSA_USB_MODE);

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

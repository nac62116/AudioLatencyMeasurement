/*
ALSA code base retrieved from https://www.linuxjournal.com/article/6735 on 7th March 2022

Makefile:
gcc -Wall -pthread latency_measurement_new.c -lasound -o latency_measurement_new -lpigpio -lrt
*/

#include <pigpio.h>
#include <alsa/asoundlib.h>
#include <stdio.h>

#define TOTAL_MEASUREMENTS 10

const int LINE_IN = 27; // GPIO 27
const int LINE_OUT = 17; // GPIO 17
//const double SIGNAL_LENGTH_IN_S = 0.001;
const double SIGNAL_LENGTH_IN_S = 0.001;
const double SIGNAL_START_INTERVAL_IN_S = 1.0;
const double SIGNAL_MINIMUM_INTERVAL_IN_S = 0.02; // Minimum interval to ensure correct amplification
const int SIGNAL_ARRIVED = 1;
const int SIGNAL_ON_THE_WAY = 0;

// User inputs
const int START_MEASUREMENT = 1; // TODO: GPIO numbers
const int START_CALIBRATION = 2;
const int LINE_OUT_MODE = 3;
const int USB_OUT_MODE = 4;
const int HDMI_OUT_MODE = 5;
const int PCIE_OUT_MODE = 6;

// Latency measurement
int measurementMode = USB_OUT_MODE;
uint32_t startTimestamp, endTimestamp;
int latencyInMicros;
int latencyMeasurementsInMicros[TOTAL_MEASUREMENTS];
int validMeasurmentsCount = 0;
int maxLatencyInMicros = -1;
int signalStatus;

// ALSA variables

/* PCM device identifier */
/* Name of the PCM device, like hw:0,0                     */
/* The first number is the number of the soundcard,        */
/* the second number is the number of the device.          */
/* In this case the lib/udev/rules.b/85-my-audio-usb.rules */
/* file changes the card id depending on the used port.    */
/* With that pcm devices can be identified like below      */
/* (hw:CARD=usb_audio_top, ...)                            */
const char *ALSA_USB_TOP_OUT = "hw:CARD=usb_audio_top";
const char *ALSA_USB_BOTTOM_OUT = "hw:CARD=usb_audio_bot";
// TODO: create udev rules for changing card ids of pcie and hdmi sound devices
const char *ALSA_HDMI1_OUT = "hw:CARD=hdmi_audio_0";
const char *ALSA_HDMI0_OUT = "hw:CARD=hdmi_audio_1";
const char *ALSA_PCIE_OUT = "hw:CARD=pcie_audio";
/* Specific hardware parameters */
const unsigned int PREFERRED_SAMPLE_RATE = 44100;
const unsigned int NUMBER_OF_CHANNELS = 2;
const long MINIMUM_NUMBER_OF_PERIODS = 25;
const snd_pcm_access_t ACCESS_TYPE = SND_PCM_ACCESS_RW_INTERLEAVED;
const snd_pcm_format_t FORMAT_TYPE = SND_PCM_FORMAT_S16_LE;
const int BYTES_PER_SAMPLE = 2; /* Depends on the format type */

// ####
// #### LOGIC ####

double calculateSignalInterval(int measurementCount) {
    double signalIntervalInS, maxLatencyInS;

    // After the first signal that arrived, the signal interval converges to the maximum measured latency
    // If its smaller than SIGNAL_MINIMUM_INTERVAL_IN_S it converges to that value
    if (maxLatencyInMicros != -1 && measurementCount > 0) {
        maxLatencyInS = (double) maxLatencyInMicros / 1000000.0;
        if (maxLatencyInS <= SIGNAL_MINIMUM_INTERVAL_IN_S) {
            signalIntervalInS = SIGNAL_MINIMUM_INTERVAL_IN_S + 1 / measurementCount * maxLatencyInS;
        }
        else {
            // The interval from the first to the second signal is SIGNAL_START_INTERVAL_IN_S
            signalIntervalInS = maxLatencyInS + 1 / measurementCount * maxLatencyInS;
        }
    }
    else {
        signalIntervalInS = SIGNAL_START_INTERVAL_IN_S;
    }
    return(signalIntervalInS);
}

// ####
// #### PCM DEVICES (USB, HDMI, PCIE) VIA ALSA ####

int startMeasurementDigitalOut() {
    double signalIntervalInS;
    int status;
    int dir;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    long numberOfPeriods;
    unsigned int periodTimeInMicros;
    unsigned int sampleRate;
    snd_pcm_uframes_t frames;
    char *buffer;
    int size;

    /* Open PCM device for playback. */ // TODO: switch HDMI_MODE USB_MODE PCI_MODE
    if (measurementMode == USB_OUT_MODE) {
        status = snd_pcm_open(&handle, ALSA_USB_TOP_OUT, SND_PCM_STREAM_PLAYBACK, 0);
        if (status < 0) {
            fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(status));
            status = snd_pcm_open(&handle, ALSA_USB_BOTTOM_OUT, SND_PCM_STREAM_PLAYBACK, 0);
            if (status < 0) {
                fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(status));
                return(-1);
            }
        }
    }
    else if (measurementMode == HDMI_OUT_MODE) {
        status = snd_pcm_open(&handle, ALSA_HDMI0_OUT, SND_PCM_STREAM_PLAYBACK, 0);
        if (status < 0) {
            fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(status));
            status = snd_pcm_open(&handle, ALSA_HDMI1_OUT, SND_PCM_STREAM_PLAYBACK, 0);
            if (status < 0) {
                fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(status));
                return(-1);
            }
        }
    }
    // PCI_MODE
    else {
        status = snd_pcm_open(&handle, ALSA_PCIE_OUT, SND_PCM_STREAM_PLAYBACK, 0);
        if (status < 0) {
            fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(status));
            return(-1);
        }
    }

    /* Allocate a hardware parameters object. */
    snd_pcm_hw_params_alloca(&params);

    /* Fill it in with default values. */
    snd_pcm_hw_params_any(handle, params);

    /* Set the desired hardware parameters. */
    snd_pcm_hw_params_set_access(handle, params, ACCESS_TYPE);
    //snd_pcm_hw_params_set_access(handle, params, SND_PCM_FORMAT_S32_LE);
    snd_pcm_hw_params_set_format(handle, params, FORMAT_TYPE);
    snd_pcm_hw_params_set_channels(handle, params, NUMBER_OF_CHANNELS);
    sampleRate = PREFERRED_SAMPLE_RATE;
    snd_pcm_hw_params_set_rate_near(handle, params, &sampleRate, &dir);
    /* Set period size to minimum to create smallest possible buffer size. */
    snd_pcm_hw_params_get_period_size_min(params, (snd_pcm_uframes_t *) &frames, &dir);
    snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);

    /* Write the parameters to the driver */
    status = snd_pcm_hw_params(handle, params);
    if (status < 0) {
        fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(status));
        return(-1);
    }

    /* Use a buffer large enough to hold one period */
    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    size = frames * BYTES_PER_SAMPLE * NUMBER_OF_CHANNELS;
    buffer = (char *) malloc(size);

    /* TODO: Fill buffer with full gain */
    for (int byte = 0; byte < size; byte++) {
        buffer[byte] = 127;
        //buffer[byte] = 0xff;
    }

    /* We want to loop for SIGNAL_LENGTH_IN_S */
    snd_pcm_hw_params_get_period_time(params, &periodTimeInMicros, &dir);
    
    for (int i = 0; i < TOTAL_MEASUREMENTS; i++) {
        signalIntervalInS = calculateSignalInterval(i);
        startTimestamp = 0;

        printf("\n\n----- Measurement %d started -----\n", i + 1);
        /* signal length in micros divided by period time */
        // TODO: Eventually greater signal length
        numberOfPeriods = SIGNAL_LENGTH_IN_S * 1000000 / periodTimeInMicros;
        if (numberOfPeriods < MINIMUM_NUMBER_OF_PERIODS) {
            numberOfPeriods = MINIMUM_NUMBER_OF_PERIODS;
        }
        printf("loops: %ld\n", numberOfPeriods);
        // TODO: Timestamp
        while (numberOfPeriods > 0) {
            status = snd_pcm_writei(handle, buffer, frames);
            if (status == -EPIPE) {
                /* EPIPE means underrun */
                fprintf(stderr, "underrun occurred\n");
                snd_pcm_prepare(handle);
            }
            else if (status < 0) {
                fprintf(stderr, "error from writei: %s\n", snd_strerror(status));
            }
            else if (status != (int)frames) {
                fprintf(stderr, "short write, write %d frames\n", status);
            }
            else {
                if (startTimestamp == 0) {
                    fprintf(stderr, "START TIMESTAMP\n");
                    startTimestamp = gpioTick();
                    signalStatus = SIGNAL_ON_THE_WAY;
                }
            }
            numberOfPeriods--;
        }
        time_sleep(signalIntervalInS);
    }

    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    free(buffer);

    return(0);
}

// ####
// #### LINE LEVEL VIA GPIOS ####

// Line-out callback
void onLineOut(int gpio, int level, uint32_t tick) {
    printf("GPIO %d state changed to level %d at %d\n", gpio, level, tick);
    printf("onLineOut");
    
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
            }
        }  
    }
}

// Line-out signal creation
int sendSignalViaLineOut(double signalIntervalInS) {
    int status = 0;

    // Send signal through LINE_OUT gpio pin
    status = gpioWrite(LINE_OUT, 1);
    time_sleep(SIGNAL_LENGTH_IN_S);
    status = gpioWrite(LINE_OUT, 0);
    time_sleep(signalIntervalInS);

    return(status);
}

int startMeasurementLineOut() {
    double signalIntervalInS;
    int status;

    for (int i = 0; i < TOTAL_MEASUREMENTS; i++) {
        signalIntervalInS = calculateSignalInterval(i);
        
        printf("\n\n----- Measurement %d started -----\n", i + 1);

        status = sendSignalViaLineOut(signalIntervalInS);
        if (status < 0) {
            return(status);
        }
    }
    return(0);
}

// TODO
void startCalibration() {
    //
}

// ####
// #### USER INTERFACE VIA GPIOS ####

void onUserInput(int gpio, int level, uint32_t tick) {
    int status;

    if (level == 1) {
        if (gpio == START_MEASUREMENT) {
            if (measurementMode == LINE_OUT_MODE) {
                status = startMeasurementLineOut();
                if (status < 0) {
                    // TODO: Display GPIO error with number status
                }
            }
            // USB_, HDMI_, PCIE_OUT
            else {
                status = startMeasurementDigitalOut();
                if (status < 0) {
                    // TODO: Display ALSA error via status variable
                }
            }
            // TODO: Saving measurements to .csv format
            // TODO: Clear measurement: descriptive values / measurements = -1 
        }
        else if (gpio == START_CALIBRATION) {
            startCalibration();
        }
        // Measurement mode got changed
        else {
            measurementMode = gpio;
        }
    }
}

int initGpioLibrary() {
    int status;

    // Initialize library
    status = gpioInitialise();

    // Set GPIO Modes
    gpioSetMode(LINE_OUT, PI_OUTPUT);
    gpioSetMode(LINE_IN, PI_INPUT);

    // Register GPIO state change callback
    gpioSetAlertFunc(LINE_OUT, onLineOut);
    gpioSetAlertFunc(LINE_IN, onLineIn);

    return(status);
}

/*void waitForUserInput() {
    while (1) {
        // Waiting for input gpio callbacks in onUserInput()
    }
}*/

int main(void) {
    int status;

    // TODO: init gpio callbacks for the user inputs
    status = initGpioLibrary();
    printf("Status after gpioInitialise: %d\n", status);

    // TODO: Function
    // Fill measurement array with -1 values to mark invalid measurements
    for (int i = 0; i < TOTAL_MEASUREMENTS; i++) {
        latencyMeasurementsInMicros[i] = -1;
    }
    
    startMeasurementDigitalOut();
    //startMeasurementLineOut();

    // waitForUserInput();
    
    // Print measurements
    for (int i = 0; i < TOTAL_MEASUREMENTS; i++) {
        printf("\n##### Measurement %d latency: %d\n", i + 1, latencyMeasurementsInMicros[i]);
    }
    
    // Terminate library
    gpioTerminate();
    
    printf("\nExit\n");
}

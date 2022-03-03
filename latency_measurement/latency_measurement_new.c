/*
ALSA code base retrieved from https://users.suse.com/~mana/alsa090_howto.html on 2nd March 2022
*/

#include <pigpio.h>
#include <alsa/asoundlib.h>
#include <stdio.h>

#define TOTAL_MEASUREMENTS 10
// BUFFER_SIZE = ALSA_PCM_PREFERRED_SAMPLE_RATE (48000 kHz) * SIGNAL_LENGTH_IN_S (0.001 s)
#define BUFFER_SIZE 1024

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
const int DISPLAY_MODE_AVERAGE = 1;
const int DISPLAY_MODE_MAXIMUM = 2;
const int DISPLAY_MODE_MINIMUM = 3;

// User inputs
const int START_MEASUREMENT = 1; // TODO: GPIO numbers
const int START_CALIBRATION = 2;
const int LINE_LEVEL_MODE = 3;
const int ALSA_PCIE_MODE = 4;
const int ALSA_USB_MODE = 5;
const int ALSA_HDMI_MODE = 6;
const int CHANGE_DISPLAY_MODE = 7;

// Latency measurement
int measurementMode = ALSA_USB_MODE;
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
int alsaStatus;
int displayModes[] = {DISPLAY_MODE_AVERAGE, DISPLAY_MODE_MAXIMUM, DISPLAY_MODE_MINIMUM};
int currentDisplayMode = DISPLAY_MODE_AVERAGE;
int displayModeCount = 0;

// ALSA variables

/* PCM device identifier */
const char *ALSA_USB_TOP_OUT = "hw:CARD=usb_audio_top";
const char *ALSA_USB_BOTTOM_OUT = "hw:CARD=usb_audio_bot";
// TODO: create udev rules for changing card ids of pcie and hdmi sound devices
const char *ALSA_HDMI1_OUT = "hw:2,0";
const char *ALSA_HDMI0_OUT = "hw:0,0";
/* Name of the PCM device, like plughw:0,0                 */
/* The first number is the number of the soundcard,        */
/* the second number is the number of the device.          */
/* In this case the lib/udev/rules.b/85-my-audio-usb.rules */
/* file changes the card id depending on the used port.    */
/* With that pcm devices can be identified like above      */
/* (hw:CARD=usb_audio_top, ...)                            */
char *pcmName;
/* Specific hardware parameters */
snd_pcm_access_t accessType;
snd_pcm_format_t formatType;
snd_pcm_uframes_t minPeriodSize;
snd_pcm_uframes_t minBufferSize;
unsigned int channels;
unsigned int sampleRate = ALSA_PCM_PREFERRED_SAMPLE_RATE;

// ####
// #### PCM DEVICES (USB, HDMI, PCIE) VIA ALSA ####

void setPCMName(const char * identifier) {
    pcmName = (char *) identifier;
}

int openPCMDevice(snd_pcm_t **pcmHandle) {
    /* Device identifier (hw:usb_audio_top, ...) */
    const char *identifier = (const char *) pcmName;

    printf("openPCMDevice\n");
    if (snd_pcm_open(pcmHandle, identifier, SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        fprintf(stderr, "Error opening PCM device %s\n", pcmName);
        return(-1);
    }
    return(0);
}

void allocateHardwareParameterStructure(snd_pcm_hw_params_t **hardwareParameters) {
    snd_pcm_hw_params_alloca(hardwareParameters);
}

int configurePCMDevice(snd_pcm_t *pcmHandle, snd_pcm_hw_params_t *hardwareParameters) {
    printf("configurePCMDevice\n");
    if (snd_pcm_hw_params_any(pcmHandle, hardwareParameters) < 0) {
        fprintf(stderr, "Error configuring PCM device %s\n", pcmName);
        snd_pcm_close(pcmHandle);
        return(-1);
    }
    return(0);
}

void getHardwareParameters(snd_pcm_hw_params_t *hardwareParameters) {
    unsigned int returnedValue;
    int direction;

    printf("getHardwareParameters");

    snd_pcm_hw_params_get_access(hardwareParameters, (snd_pcm_access_t *) &returnedValue);
    accessType = (snd_pcm_access_t) returnedValue;

    snd_pcm_hw_params_get_format(hardwareParameters, (snd_pcm_format_t *) &returnedValue);
    formatType = (snd_pcm_format_t) returnedValue;

    snd_pcm_hw_params_get_channels(hardwareParameters, &returnedValue);
    channels = returnedValue;

    snd_pcm_hw_params_get_rate(hardwareParameters, &returnedValue, &direction);
    sampleRate = returnedValue;

    snd_pcm_hw_params_get_period_size_min(hardwareParameters, (snd_pcm_uframes_t *) &returnedValue, &direction);
    minPeriodSize = (snd_pcm_uframes_t) returnedValue;

    snd_pcm_hw_params_get_buffer_size_min(hardwareParameters, (snd_pcm_uframes_t *) &returnedValue);
    minBufferSize = (snd_pcm_uframes_t) returnedValue;

    printf("\naccess type: %d\n\n", accessType);
    printf("\format type: %d\n\n", formatType);
    printf("\nchannels: %d\n\n", channels);
    printf("\nsample rate: %d\n\n", sampleRate);
    printf("\nmin period size: %ld\n\n", minPeriodSize);
    printf("\nmin buffer size: %ld\n\n", minBufferSize);
}

void setHardwareParameters() {
    //
}

int initPCMDevice(const char *identifier) {
    int status = 0;
    /* Handle for the PCM device */
    snd_pcm_t *pcmHandle;
    /* This structure contains information about    */
    /* the hardware and can be used to specify the  */      
    /* configuration to be used for the PCM stream. */ 
    snd_pcm_hw_params_t *hardwareParameters;

    setPCMName(identifier);
    status = openPCMDevice(&pcmHandle);
    if (status != -1) {
        allocateHardwareParameterStructure(&hardwareParameters);
        status = configurePCMDevice(pcmHandle, hardwareParameters);
        if (status != -1) {
            getHardwareParameters(hardwareParameters);
            setHardwareParameters();
        }
    }

    return(status);
}

void sendSignalViaPCMDevice(double signalIntervalInS) {
    
    signalStatus = SIGNAL_ON_THE_WAY;
    // Send signal through alsa pcm device
    // TODO

    // Start measurement
    startTimestamp = gpioTick();
    time_sleep(SIGNAL_LENGTH_IN_S);
    //snd_pcm_drain(pcmHandle);
    time_sleep(signalIntervalInS);
}

// ####
// #### LINE LEVEL VIA GPIOS ####

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

                // TODO: displayDescriptiveValues(currentDisplayMode, avg, max, min)
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

// ####
// #### LOGIC ####

void startMeasurement() {
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

        printf("\n\n----- Measurement %d started -----\n", i + 1);
        if (measurementMode == LINE_LEVEL_MODE) {
            sendSignalViaLineOut(signalIntervalInS);
        }
        // TODO: || HDMI_MODE || PCIE_MODE ... or else {}
        else if (measurementMode == ALSA_USB_MODE) {
            sendSignalViaPCMDevice(signalIntervalInS);
        }
        // TODO: else if (measurementMode == USB, PCIe...
        else {}
    }
    // TODO: Saving measurements to .csv format
}

// TODO
void startCalibration() {
    //
}

// ####
// #### USER INTERFACE VIA GPIOS ####

void onUserInput(int gpio, int level, uint32_t tick) {

    if (level == 1) {
        if (gpio == START_MEASUREMENT) {
            startMeasurement();
        }
        else if (gpio == START_CALIBRATION) {
            startCalibration();
        }
        else if (gpio == CHANGE_DISPLAY_MODE) {
            // TODO: Function
            if (displayModeCount == sizeof(displayModes) / sizeof(displayModes[0]) - 1) {
                displayModeCount = 0;
            }
            else {
                displayModeCount += 1;
            }
            currentDisplayMode = displayModes[displayModeCount];
        }
        // Measurement mode got changed
        else {
            measurementMode = gpio;
            if (gpio == ALSA_USB_MODE) {
                // TODO: Function
                alsaStatus = initPCMDevice(ALSA_USB_TOP_OUT);
                if (alsaStatus == -1) {
                    alsaStatus = initPCMDevice(ALSA_USB_BOTTOM_OUT);
                    if (alsaStatus == -1) {
                        // TODO: Display error message: usb audio device not found
                    }
                }
            }
            else if (gpio == ALSA_HDMI_MODE) {
                //
            }
            // TODO: measurementMode changes
            else {}
        }
    }
}

void initGpioLibrary() {

    // If library is still initialised, terminate library
    gpioTerminate();

    // Initialize library
    gpioStatus = gpioInitialise();
    printf("Status after gpioInitialise: %d\n", gpioStatus);

    // Set GPIO Modes
    gpioSetMode(LINE_OUT, PI_OUTPUT);
    gpioSetMode(LINE_IN, PI_INPUT);

    // Register GPIO state change callback
    gpioSetAlertFunc(LINE_OUT, onLineOut);
    gpioSetAlertFunc(LINE_IN, onLineIn);
}

/*void waitForUserInput() {
    while (1) {
        // Waiting for input gpio callbacks in onUserInput()
    }
}*/

int main(void) {

    // TODO: init gpio callbacks for the user inputs
    initGpioLibrary();
    
    // TODO: Remove this and set LINE_LEVEL_MODE as default mode
    alsaStatus = initPCMDevice(ALSA_USB_TOP_OUT);
    if (alsaStatus == -1) {
        alsaStatus = initPCMDevice(ALSA_USB_BOTTOM_OUT);
        if (alsaStatus == -1) {
            // TODO: Display error message: usb audio device not found
        }
    }

    // TODO: Function
    // Fill measurement array with -1 values to mark invalid measurements
    for (int i = 0; i < TOTAL_MEASUREMENTS; i++) {
        latencyMeasurementsInMicros[i] = -1;
    }

    // waitForUserInput();
    // startMeasurement();

    // TODO: Remove status variable or handle errors
    printf("\nGPIO STATUS: %d\n", gpioStatus);
    
    // Print measurements
    for (int i = 0; i < TOTAL_MEASUREMENTS; i++) {
        printf("\n##### Measurement %d latency: %d\n", i + 1, latencyMeasurementsInMicros[i]);
    }
    
    // Terminate library
    gpioTerminate();
    
    printf("\nExit\n");
}

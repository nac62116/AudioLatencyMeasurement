/*
ALSA code base retrieved from https://www.linuxjournal.com/article/6735 on 7th March 2022

Makefile:
gcc -Wall -pthread latency_measurement_new.c -lasound -o latency_measurement_new -lpigpio -lrt
*/

#include <pigpio.h>
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <time.h>

#define TOTAL_MEASUREMENTS 100
#define TOTAL_CALIBRATION_MEASUREMENTS 10

// Line level in and output
const int LINE_IN = 4; // GPIO 4
const int LINE_OUT = 5; // GPIO 5

// User inputs
const int START_MEASUREMENT_BUTTON = 9; // GPIO 9
const int CALIBRATION_MODE_BUTTON = 10; // GPIO 10
const int LINE_OUT_MODE_BUTTON = 15; // GPIO 15
const int USB_OUT_MODE_BUTTON = 23; // GPIO 23
const int HDMI_OUT_MODE_BUTTON = 25; // GPIO 25
const int PCIE_OUT_MODE_BUTTON = 7; // GPIO 7

// User feedback
const int START_MEASUREMENT_LED = 11; // GPIO 11
const int CALIBRATION_MODE_RED_LED = 22; // GPIO 22
const int CALIBRATION_MODE_YELLOW_LED = 27; // GPIO 27
const int CALIBRATION_MODE_GREEN_LED = 17; // GPIO 17
const int LINE_OUT_MODE_LED = 14; // GPIO 14
const int USB_OUT_MODE_LED = 18; // GPIO 18
const int HDMI_OUT_MODE_LED = 24; // GPIO 24
const int PCIE_OUT_MODE_LED = 8; // GPIO 8

// Latency measurement
const double SIGNAL_LENGTH_IN_S = 0.001;
const double SIGNAL_START_INTERVAL_IN_S = 0.1;
const double SIGNAL_MINIMUM_INTERVAL_IN_S = 0.02; // Minimum interval to ensure correct amplification
const int SIGNAL_ARRIVED = 1;
const int SIGNAL_ON_THE_WAY = 0;
const int CALIBRATE = 0;
const int MEASURE = 1;
int measurementMode = LINE_OUT_MODE_BUTTON;
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
unsigned int sampleRate;
int bufferSize;

// File creation
const char *FILE_NAME_PREFIX_LINE_TO_LINE = "line-to-line_";
const char *FILE_NAME_PREFIX_USB_TO_LINE = "usb-to-line_";
const char *FILE_NAME_PREFIX_HDMI_TO_LINE = "hdmi-to-line_";
const char *FILE_NAME_PREFIX_PCIE_TO_LINE = "pcie-to-line_";
const char *FILE_NAME_SUFFIX_NO_TIMESTAMP = "no-timestamp";
const char *FILE_TYPE_SUFFIX = ".csv";
const char *DUT_OUTPUT_VALUE_LINE = "LINE OUT";
const char *DUT_INPUT_VALUE_LINE = "LINE IN";
const char *DUT_INPUT_VALUE_USB = "USB IN";
const char *DUT_INPUT_VALUE_HDMI = "HDMI IN";
const char *DUT_INPUT_VALUE_PCIE = "PCIE IN";
const char *MEASUREMENTS_FOLDER_PATH = "/home/pi/Desktop/AudioLatencyMeasurement/measurements/";
const char *CSV_HEADER = "LATENCY_IN_MICROS,DUT_INPUT,DUT_OUTPUT,BUFFER_SIZE,SAMPLE_RATE,CHANNELS\n";

// ####
// #### LOGIC ####

void initMeasurement() {
    validMeasurmentsCount = 0;
    maxLatencyInMicros = -1;
    // Fill measurement array with -1 values to mark invalid measurements
    for (int i = 0; i < TOTAL_MEASUREMENTS; i++) {
        latencyMeasurementsInMicros[i] = -1;
    }
}

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

void getMeasurementDependentValuesForCSV(char *fileName, char *dutInput, char *dutOutput) {
    const char *fileNamePrefix;

    if (measurementMode == LINE_OUT_MODE_BUTTON) {
        fileNamePrefix = FILE_NAME_PREFIX_LINE_TO_LINE;
        strcpy(dutInput, DUT_INPUT_VALUE_LINE);
    }
    else if (measurementMode == USB_OUT_MODE_BUTTON) {
        fileNamePrefix = FILE_NAME_PREFIX_USB_TO_LINE;
        strcpy(dutInput, DUT_INPUT_VALUE_USB);
    }
    else if (measurementMode == HDMI_OUT_MODE_BUTTON) {
        fileNamePrefix = FILE_NAME_PREFIX_HDMI_TO_LINE;
        strcpy(dutInput, DUT_INPUT_VALUE_HDMI);
    }
    else {
        fileNamePrefix = FILE_NAME_PREFIX_PCIE_TO_LINE;
        strcpy(dutInput, DUT_INPUT_VALUE_PCIE);
    }
    strcpy(fileName, fileNamePrefix);
    strcpy(dutOutput, DUT_OUTPUT_VALUE_LINE);
}

void usePigpioForTimestamp(char *fileName) {
    int secondsSinceEpoch;
    char secondsSinceEpochString[1024];
    int microsSinceEpoch;

    if (gpioTime(PI_TIME_ABSOLUTE, &secondsSinceEpoch, &microsSinceEpoch) == 0) {
        sprintf(secondsSinceEpochString, "%d", secondsSinceEpoch);
        strcat(fileName, (const char *) secondsSinceEpochString);
    }
    else {
        strcat(fileName, FILE_NAME_SUFFIX_NO_TIMESTAMP);
    }
}

void addTimestampToFileName(char *fileName) {
    time_t currentTime;
    char *currentTimeString;

    currentTime = time(NULL);

    if (currentTime == ((time_t)-1)) {
        usePigpioForTimestamp(fileName);
    }
    else {
        /* Convert to local time format. */
        currentTimeString = ctime(&currentTime);

        if (currentTimeString == NULL) {
            usePigpioForTimestamp(fileName);
        }
        else {
            strcat(fileName, currentTimeString);
        }
    }
}

void writeMeasurementsToCSV() {
    FILE *filePointer;
    char fileName[1024];
    char dutInput[1024];
    char dutOutput[1024];
    char measurementsFolderPath[1024];

    getMeasurementDependentValuesForCSV(fileName, dutInput, dutOutput);
    addTimestampToFileName(fileName);
    // Adding file type .csv and writing csv file
    strcat(fileName, FILE_TYPE_SUFFIX);
    // Appending file name to measurements folder path
    strcpy(measurementsFolderPath, MEASUREMENTS_FOLDER_PATH);
    strcat(measurementsFolderPath, fileName);
    filePointer = fopen(measurementsFolderPath, "w");

    if (filePointer != NULL) {
        fprintf(filePointer, CSV_HEADER);
        for (int i = 0; i < TOTAL_MEASUREMENTS; i++) {
            fprintf(filePointer, "%d,%s,%s,%d,%d,%d\n",
                    latencyMeasurementsInMicros[i],
                    dutInput,
                    dutOutput,
                    bufferSize,
                    sampleRate,
                    NUMBER_OF_CHANNELS);
        }
        fclose(filePointer);
    }
    else {
        printf("audio_lag_module.c l.217: Could not open file\n");
    }
}

// ####
// #### LINE LEVEL VIA GPIOS ####

// Line-in callback
void onLineIn(int gpio, int level, uint32_t tick) {
    
    // Rising Edge
    if (level == 1) {

        // This condition avoids, that multiple trigger of the transistor lead to reassignment of the endTimestamp
        if (signalStatus == SIGNAL_ON_THE_WAY) {
            endTimestamp = tick;
            signalStatus = SIGNAL_ARRIVED;
            gpioSetAlertFunc(LINE_IN, NULL);

            latencyInMicros = endTimestamp - startTimestamp;

            // The uint32_t tick parameter represents the number of microseconds since boot.
            // This wraps around from 4294967295 to 0 approximately every 72 minutes.
            // Thats why the provided latency could be both negative and wrong in this specific situation.
            if (latencyInMicros >= 0) {
                
                // Saving valid measurement
                latencyMeasurementsInMicros[validMeasurmentsCount] = latencyInMicros;
                validMeasurmentsCount += 1;
                
                // Updating maximum latency
                if (maxLatencyInMicros == -1) {
                    maxLatencyInMicros = latencyInMicros;
                }
                else if (latencyInMicros > maxLatencyInMicros) {
                    maxLatencyInMicros = latencyInMicros;
                }
                else {
                    // Current latency is smaller than the maximum latency
                }
            }
        }  
    }
}

// Line-out callback
void onLineOut(int gpio, int level, uint32_t tick) {
    
    // Rising Edge
    if (level == 1) {
        startTimestamp = tick;
        signalStatus = SIGNAL_ON_THE_WAY;
    }
}


// Line-out signal creation
void sendSignalViaLineOut(double signalIntervalInS) {
    // Send signal through LINE_OUT gpio pin
    gpioWrite(LINE_OUT, 1);
    time_sleep(SIGNAL_LENGTH_IN_S);
    gpioWrite(LINE_OUT, 0);
    time_sleep(signalIntervalInS);
}

void startMeasurementLineOut(int measurementMethod) {
    double signalIntervalInS;
    int iterations;

    if (measurementMethod == CALIBRATE) {
        iterations = TOTAL_CALIBRATION_MEASUREMENTS;
    }
    else {
        iterations = TOTAL_MEASUREMENTS;
    }
    for (int i = 0; i < iterations; i++) {

        if (measurementMethod == MEASURE) {
            signalIntervalInS = calculateSignalInterval(i);
        }
        else {
            signalIntervalInS = SIGNAL_START_INTERVAL_IN_S;
        }

        gpioSetAlertFunc(LINE_IN, onLineIn);
        sendSignalViaLineOut(signalIntervalInS);
    }
}

void initGPIOs() {

    // Initialize library
    gpioInitialise();

    // Set GPIO Modes
    gpioSetMode(LINE_OUT, PI_OUTPUT);
    gpioSetMode(LINE_IN, PI_INPUT);
    gpioSetMode(START_MEASUREMENT_BUTTON, PI_INPUT);
    gpioSetMode(CALIBRATION_MODE_BUTTON, PI_INPUT);
    gpioSetMode(LINE_OUT_MODE_BUTTON, PI_INPUT);
    gpioSetMode(USB_OUT_MODE_BUTTON, PI_INPUT);
    gpioSetMode(HDMI_OUT_MODE_BUTTON, PI_INPUT);
    gpioSetMode(PCIE_OUT_MODE_BUTTON, PI_INPUT);
    gpioSetMode(START_MEASUREMENT_LED, PI_OUTPUT);
    gpioSetMode(CALIBRATION_MODE_GREEN_LED, PI_OUTPUT);
    gpioSetMode(CALIBRATION_MODE_YELLOW_LED, PI_OUTPUT);
    gpioSetMode(CALIBRATION_MODE_RED_LED, PI_OUTPUT);
    gpioSetMode(LINE_OUT_MODE_LED, PI_OUTPUT);
    gpioSetMode(USB_OUT_MODE_LED, PI_OUTPUT);
    gpioSetMode(HDMI_OUT_MODE_LED, PI_OUTPUT);
    gpioSetMode(PCIE_OUT_MODE_LED, PI_OUTPUT);

    // Register GPIO state change callback
    gpioSetAlertFunc(LINE_OUT, onLineOut);
    gpioSetAlertFunc(LINE_IN, onLineIn);

    // Initial measurement mode
    gpioWrite(LINE_OUT_MODE_LED, 1);
    // For strange debugging reasons
    gpioWrite(LINE_IN, 0);
}

// ####
// #### PCM DEVICES (USB, HDMI, PCIE) VIA ALSA ####

void startMeasurementDigitalOut(int measurementMethod) {
    double signalIntervalInS;
    int status;
    int dir;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    unsigned int periodTimeInMicros;
    snd_pcm_uframes_t frames;
    long numberOfPeriods;
    char *buffer;
    int iterations;

    if (measurementMethod == CALIBRATE) {
        iterations = TOTAL_CALIBRATION_MEASUREMENTS;
    }
    else {
        iterations = TOTAL_MEASUREMENTS;
    }

    // Open PCM device for playback. 
    if (measurementMode == USB_OUT_MODE_BUTTON) {
        status = snd_pcm_open(&handle, ALSA_USB_TOP_OUT, SND_PCM_STREAM_PLAYBACK, 0);
        if (status < 0) {
            // Unable to open pcm device
            status = snd_pcm_open(&handle, ALSA_USB_BOTTOM_OUT, SND_PCM_STREAM_PLAYBACK, 0);
            if (status < 0) {
                printf("audio_lag_module.c l.368: Unable to open PCM Device\n");
                return;
            }
        }
    }
    else if (measurementMode == HDMI_OUT_MODE_BUTTON) {
        status = snd_pcm_open(&handle, ALSA_HDMI0_OUT, SND_PCM_STREAM_PLAYBACK, 0);
        if (status < 0) {
            // Unable to open pcm device
            status = snd_pcm_open(&handle, ALSA_HDMI1_OUT, SND_PCM_STREAM_PLAYBACK, 0);
            if (status < 0) {
                printf("audio_lag_module.c l.379: Unable to open PCM Device\n");
                return;
            }
        }
    }
    // PCI_MODE
    else {
        status = snd_pcm_open(&handle, ALSA_PCIE_OUT, SND_PCM_STREAM_PLAYBACK, 0);
        if (status < 0) {
            printf("audio_lag_module.c l.388: Unable to open PCM Device\n");
            return;
        }
    }

    // Allocate a hardware parameters object. 
    snd_pcm_hw_params_alloca(&params);

    // Fill it in with default values. 
    snd_pcm_hw_params_any(handle, params);

    // Set the desired hardware parameters. 
    snd_pcm_hw_params_set_access(handle, params, ACCESS_TYPE);
    //snd_pcm_hw_params_set_access(handle, params, SND_PCM_FORMAT_S32_LE);
    snd_pcm_hw_params_set_format(handle, params, FORMAT_TYPE);
    snd_pcm_hw_params_set_channels(handle, params, NUMBER_OF_CHANNELS);
    sampleRate = PREFERRED_SAMPLE_RATE;
    snd_pcm_hw_params_set_rate_near(handle, params, &sampleRate, &dir);
    // Set period size to minimum to create smallest possible buffer size.
    snd_pcm_hw_params_get_period_size_min(params, (snd_pcm_uframes_t *) &frames, &dir);
    snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);

    // Write the parameters to the driver 
    status = snd_pcm_hw_params(handle, params);
    if (status < 0) {
        printf("audio_lag_module.c l.413: Unable to set PCM devices hardware parameters\n");
        return;
    }
    
    // Use a buffer large enough to hold one period 
    snd_pcm_hw_params_get_period_size(params, &frames, &dir);
    bufferSize = frames * BYTES_PER_SAMPLE * NUMBER_OF_CHANNELS;
    buffer = (char *) malloc(bufferSize);

    // Fill audio buffer
    for (int byte = 0; byte < bufferSize; byte++) {
        buffer[byte] = 127;
    }
    
    /* We want to loop for SIGNAL_LENGTH_IN_S */
    snd_pcm_hw_params_get_period_time(params, &periodTimeInMicros, &dir);
    
    for (int i = 0; i < iterations; i++) {
        if (measurementMethod == MEASURE) {
            signalIntervalInS = calculateSignalInterval(i);
        }
        else {
            signalIntervalInS = SIGNAL_START_INTERVAL_IN_S;
        }

        numberOfPeriods = SIGNAL_LENGTH_IN_S * 1000000 / periodTimeInMicros;
        if (numberOfPeriods < MINIMUM_NUMBER_OF_PERIODS) {
            numberOfPeriods = MINIMUM_NUMBER_OF_PERIODS;
        }
        while (numberOfPeriods > 0) {
            status = snd_pcm_writei(handle, buffer, frames);
            if (status == -EPIPE) {
                // EPIPE means underrun 
                snd_pcm_prepare(handle);
            }
            else if (status < 0) {
                // Error from writei
            }
            else if (status != (int)frames) {
                // Short write
            }
            else {
                if (signalStatus != SIGNAL_ON_THE_WAY) {
                    startTimestamp = gpioTick();
                    signalStatus = SIGNAL_ON_THE_WAY;
                    gpioSetAlertFunc(LINE_IN, onLineIn);
                }
            }
            numberOfPeriods--;
        }
        time_sleep(signalIntervalInS);
    }

    snd_pcm_drain(handle);
    snd_pcm_close(handle);
    free(buffer);
}

// ####
// #### USER INTERFACE VIA GPIOS ####

void userFeedbackGoodSignal() {
    gpioWrite(CALIBRATION_MODE_GREEN_LED, 1);
    gpioWrite(CALIBRATION_MODE_YELLOW_LED, 1);
    gpioWrite(CALIBRATION_MODE_RED_LED, 1);
}

void userFeedbackMediumSignal() {
    gpioWrite(CALIBRATION_MODE_GREEN_LED, 0);
    gpioWrite(CALIBRATION_MODE_YELLOW_LED, 1);
    gpioWrite(CALIBRATION_MODE_RED_LED, 1);
}

void userFeedbackBadSignal() {
    gpioWrite(CALIBRATION_MODE_GREEN_LED, 0);
    gpioWrite(CALIBRATION_MODE_YELLOW_LED, 0);
    gpioWrite(CALIBRATION_MODE_RED_LED, 1);
}

void userFeedbackCalibrationCancelled() {
    gpioWrite(CALIBRATION_MODE_GREEN_LED, 0);
    gpioWrite(CALIBRATION_MODE_YELLOW_LED, 0);
    gpioWrite(CALIBRATION_MODE_RED_LED, 0);
}

void turnOffAllButtonLEDs() {
    gpioWrite(LINE_OUT_MODE_LED, 0);
    gpioWrite(USB_OUT_MODE_LED, 0);
    gpioWrite(HDMI_OUT_MODE_LED, 0);
    gpioWrite(PCIE_OUT_MODE_LED, 0);
}

void waitForUserInput() {
    while (1) {
        if (gpioRead(START_MEASUREMENT_BUTTON) == 1) {
            gpioWrite(START_MEASUREMENT_LED, 1);
            initMeasurement();
            if (measurementMode == LINE_OUT_MODE_BUTTON) {
                startMeasurementLineOut(MEASURE);
            }
            // USB_, HDMI_, PCIE_OUT
            else {
                startMeasurementDigitalOut(MEASURE);
            }
            writeMeasurementsToCSV();
            gpioWrite(START_MEASUREMENT_LED, 0);
            // TODO: Remove this
            // Print measurements
            for (int i = 0; i < TOTAL_MEASUREMENTS; i++) {
                printf("\n##### Measurement %d latency: %d\n", i + 1, latencyMeasurementsInMicros[i]);
            }
            
        }
        else if (gpioRead(CALIBRATION_MODE_BUTTON) == 1) {
            while (gpioRead(START_MEASUREMENT_BUTTON) == 0
                    && gpioRead(LINE_OUT_MODE_BUTTON) == 0
                    && gpioRead(USB_OUT_MODE_BUTTON) == 0
                    && gpioRead(HDMI_OUT_MODE_BUTTON) == 0
                    && gpioRead(PCIE_OUT_MODE_BUTTON) == 0) {
                initMeasurement();
                if (measurementMode == LINE_OUT_MODE_BUTTON) {
                    startMeasurementLineOut(CALIBRATE);
                }
                // USB_, HDMI_, PCIE_OUT
                else {
                    startMeasurementDigitalOut(CALIBRATE);
                }
                if (validMeasurmentsCount == TOTAL_CALIBRATION_MEASUREMENTS) {
                    userFeedbackGoodSignal();
                }
                else if (validMeasurmentsCount > TOTAL_CALIBRATION_MEASUREMENTS / 2) {
                    userFeedbackMediumSignal();
                }
                else {
                    userFeedbackBadSignal();
                }
                /* TODO: Test without this
                for (int millis = 0; millis <= 1000; millis++) {
                    time_sleep(0.001);
                }*/
            }
            userFeedbackCalibrationCancelled();
        }
        // Measurement mode got changed
        // Duplicate code could not be avoided here.
        else if (gpioRead(LINE_OUT_MODE_BUTTON) == 1) {
            turnOffAllButtonLEDs();
            measurementMode = LINE_OUT_MODE_BUTTON;
            gpioWrite(LINE_OUT_MODE_LED, 1);
        }
        else if (gpioRead(USB_OUT_MODE_BUTTON) == 1) {
            turnOffAllButtonLEDs();
            measurementMode = USB_OUT_MODE_BUTTON;
            gpioWrite(USB_OUT_MODE_LED, 1);
        }
        else if (gpioRead(HDMI_OUT_MODE_BUTTON) == 1) {
            turnOffAllButtonLEDs();
            measurementMode = HDMI_OUT_MODE_BUTTON;
            gpioWrite(HDMI_OUT_MODE_LED, 1);
        }
        else if (gpioRead(PCIE_OUT_MODE_BUTTON) == 1) {
            turnOffAllButtonLEDs();
            measurementMode = PCIE_OUT_MODE_BUTTON;
            gpioWrite(PCIE_OUT_MODE_LED, 1);
            // TODO: Remove this
            return;
        }
        else {
            // No action, just keeping the while loop going
        }
    }
}

int main(void) {

    initGPIOs();

    /* TODO: Test without this
    gpioWrite(LINE_OUT_MODE_LED, 1);
    gpioWrite(LINE_IN, 0);
    */

    waitForUserInput();

    // TODO: Remove this
    gpioSetMode(LINE_OUT, PI_OUTPUT);
    gpioSetMode(LINE_IN, PI_OUTPUT);
    gpioSetMode(START_MEASUREMENT_BUTTON, PI_OUTPUT);
    gpioSetMode(CALIBRATION_MODE_BUTTON, PI_OUTPUT);
    gpioSetMode(LINE_OUT_MODE_BUTTON, PI_OUTPUT);
    gpioSetMode(USB_OUT_MODE_BUTTON, PI_OUTPUT);
    gpioSetMode(HDMI_OUT_MODE_BUTTON, PI_OUTPUT);
    gpioSetMode(PCIE_OUT_MODE_BUTTON, PI_OUTPUT);

    gpioWrite(LINE_OUT, 0);
    gpioWrite(LINE_IN, 0);
    gpioWrite(START_MEASUREMENT_BUTTON, 0);
    gpioWrite(CALIBRATION_MODE_BUTTON, 0);
    gpioWrite(LINE_OUT_MODE_BUTTON, 0);
    gpioWrite(USB_OUT_MODE_BUTTON, 0);
    gpioWrite(HDMI_OUT_MODE_BUTTON, 0);
    gpioWrite(PCIE_OUT_MODE_BUTTON, 0);
    gpioWrite(START_MEASUREMENT_LED, 0);
    gpioWrite(CALIBRATION_MODE_GREEN_LED, 0);
    gpioWrite(CALIBRATION_MODE_YELLOW_LED, 0);
    gpioWrite(CALIBRATION_MODE_RED_LED, 0);
    gpioWrite(LINE_OUT_MODE_LED, 0);
    gpioWrite(USB_OUT_MODE_LED, 0);
    gpioWrite(HDMI_OUT_MODE_LED, 0);
    gpioWrite(PCIE_OUT_MODE_LED, 0);
    
    // Terminate library
    gpioTerminate();
    
    printf("\nExit\n");
}

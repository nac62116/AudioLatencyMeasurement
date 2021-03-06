/*
ALSA code base retrieved from https://www.linuxjournal.com/article/6735 on 7th March 2022

Makefile:
gcc -Wall -pthread latency_measurement_new.c -lasound -o latency_measurement_new -lpigpio -lrt
*/

#include <pigpio.h>
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <time.h>

// Line level in and output
#define LINE_IN 4 // GPIO 4
#define LINE_OUT 5 // GPIO 5

// User inputs
#define START_MEASUREMENT_BUTTON 9 // GPIO 9
#define CALIBRATION_MODE_BUTTON 10 // GPIO 10
#define LINE_OUT_MODE_BUTTON 15 // GPIO 15
#define USB_OUT_MODE_BUTTON 23 // GPIO 23
#define HDMI_OUT_MODE_BUTTON 25 // GPIO 25
#define EXIT_BUTTON 7 // GPIO 7

// User feedback
#define START_MEASUREMENT_LED 11 // GPIO 11
#define CALIBRATION_MODE_RED_LED 22 // GPIO 22
#define CALIBRATION_MODE_YELLOW_LED 27 // GPIO 27
#define CALIBRATION_MODE_GREEN_LED 17 // GPIO 17
#define LINE_OUT_MODE_LED 14 // GPIO 14
#define USB_OUT_MODE_LED 18 // GPIO 18
#define HDMI_OUT_MODE_LED 24 // GPIO 24
#define EXIT_LED 8 // GPIO 8

// Latency measurement
#define TOTAL_MEASUREMENTS 1000
#define TOTAL_CALIBRATION_MEASUREMENTS 10
#define SIGNAL_LENGTH_IN_S 0.001
#define SIGNAL_START_INTERVAL_IN_S 0.1
#define SIGNAL_MINIMUM_INTERVAL_IN_S 0.02 // Minimum interval to ensure correct amplification
#define SIGNAL_ARRIVED 1
#define SIGNAL_ON_THE_WAY 0
#define CALIBRATE 0
#define MEASURE 1
int measurementMode = LINE_OUT_MODE_BUTTON;
uint32_t startTimestamp, endTimestamp;
int latencyInMicros;
int latencyMeasurementsInMicros[TOTAL_MEASUREMENTS];
int validMeasurementsCount = 0;
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
#define ALSA_USB_TOP_OUT "hw:CARD=usb_audio_top"
#define ALSA_USB_TOP2_OUT "hw:CARD=usb_audio_top2"
#define ALSA_USB_BOTTOM_OUT "hw:CARD=usb_audio_bot"
#define ALSA_USB_BOTTOM2_OUT "hw:CARD=usb_audio_bot2"
#define ALSA_HDMI_OUT "hw:CARD=vc4hdmi"
/* Specific hardware parameters */
#define PREFERRED_SAMPLE_RATE 44100
#define NUMBER_OF_CHANNELS 1
#define MINIMUM_NUMBER_OF_PERIODS 25 // To ensure long enough signal
#define BYTES_PER_SAMPLE 2 /* Depends on the format type */
snd_pcm_access_t ACCESS_TYPE = SND_PCM_ACCESS_RW_INTERLEAVED;
snd_pcm_format_t FORMAT_TYPE = SND_PCM_FORMAT_S16_LE;
unsigned int sampleRate;
int bufferSize;

// File creation
#define FILE_NAME_PREFIX_LINE_TO_LINE "line-to-line_"
#define FILE_NAME_PREFIX_USB_TO_LINE "usb-to-line_"
#define FILE_NAME_PREFIX_HDMI_TO_LINE "hdmi-to-line_"
#define FILE_NAME_SUFFIX_NO_TIMESTAMP "no-timestamp"
#define FILE_TYPE_SUFFIX ".csv"
#define DUT_OUTPUT_VALUE_LINE "LINE OUT"
#define DUT_INPUT_VALUE_LINE "LINE IN"
#define DUT_INPUT_VALUE_USB "USB IN"
#define DUT_INPUT_VALUE_HDMI "HDMI IN"
#define MEASUREMENTS_FOLDER_PATH "/home/pi/Desktop/AudioLatencyMeasurement/measurements/"
#define CSV_HEADER "LATENCY_IN_MICROS,DUT_INPUT,DUT_OUTPUT,BUFFER_SIZE,SAMPLE_RATE,CHANNELS\n"

// ####
// #### LOGIC ####

void resetMeasurement() {
    validMeasurementsCount = 0;
    maxLatencyInMicros = -1;
    bufferSize = 0;
    sampleRate = 0;
    // Fill measurement array with -1 values to mark invalid measurements
    for (int i = 0; i < TOTAL_MEASUREMENTS; i++) {
        latencyMeasurementsInMicros[i] = -1;
    }
}

double calculateSignalInterval(int measurementCount) {
    double signalIntervalInS, maxLatencyInS;

    // After the first signal that arrived, the signal interval converges to the maximum measured latency
    // If its smaller than SIGNAL_MINIMUM_INTERVAL_IN_S it converges to that value
    maxLatencyInS = (double) maxLatencyInMicros / 1000000.0;
    if (maxLatencyInMicros != -1 
        && maxLatencyInS < SIGNAL_START_INTERVAL_IN_S
        && measurementCount > 0) {
        if (maxLatencyInS <= SIGNAL_MINIMUM_INTERVAL_IN_S) {
            signalIntervalInS = SIGNAL_MINIMUM_INTERVAL_IN_S + 1 / measurementCount * maxLatencyInS;
        }
        else {
            signalIntervalInS = maxLatencyInS + 1 / measurementCount * maxLatencyInS;
        }
    }
    // The interval from the first to the second signal is SIGNAL_START_INTERVAL_IN_S
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
    else {
        fileNamePrefix = FILE_NAME_PREFIX_HDMI_TO_LINE;
        strcpy(dutInput, DUT_INPUT_VALUE_HDMI);
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
    int i = 0;

    getMeasurementDependentValuesForCSV(fileName, dutInput, dutOutput);
    addTimestampToFileName(fileName);
    // Removing ":" character to make it windows compatible
    while (fileName[i] != '\0') {
        if (fileName[i] == ':') {
            fileName[i] = '_';
        }
        if (fileName[i] == '\n') {
            fileName[i] = ' ';
        }
        i++;
    }
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
        printf("audio_lag_module.c l.231: Could not open file\n");
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
            if (latencyInMicros >= 0 && validMeasurementsCount < TOTAL_MEASUREMENTS) {
                
                // Saving valid measurement
                latencyMeasurementsInMicros[validMeasurementsCount] = latencyInMicros;
                validMeasurementsCount += 1;
                
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
        printf("### Measurement %d\n", i);
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

    // Initialise library
    gpioInitialise();

    // Set GPIO Modes
    gpioSetMode(LINE_OUT, PI_OUTPUT);
    gpioSetMode(LINE_IN, PI_INPUT);
    gpioSetMode(START_MEASUREMENT_BUTTON, PI_INPUT);
    gpioSetMode(CALIBRATION_MODE_BUTTON, PI_INPUT);
    gpioSetMode(LINE_OUT_MODE_BUTTON, PI_INPUT);
    gpioSetMode(USB_OUT_MODE_BUTTON, PI_INPUT);
    gpioSetMode(HDMI_OUT_MODE_BUTTON, PI_INPUT);
    gpioSetMode(EXIT_BUTTON, PI_INPUT);
    gpioSetMode(START_MEASUREMENT_LED, PI_OUTPUT);
    gpioSetMode(CALIBRATION_MODE_GREEN_LED, PI_OUTPUT);
    gpioSetMode(CALIBRATION_MODE_YELLOW_LED, PI_OUTPUT);
    gpioSetMode(CALIBRATION_MODE_RED_LED, PI_OUTPUT);
    gpioSetMode(LINE_OUT_MODE_LED, PI_OUTPUT);
    gpioSetMode(USB_OUT_MODE_LED, PI_OUTPUT);
    gpioSetMode(HDMI_OUT_MODE_LED, PI_OUTPUT);
    gpioSetMode(EXIT_LED, PI_OUTPUT);

    // Register GPIO state change callback
    gpioSetAlertFunc(LINE_OUT, onLineOut);
    gpioSetAlertFunc(LINE_IN, onLineIn);

    // Initial measurement mode
    gpioWrite(LINE_OUT_MODE_LED, 1);
    // For strange debugging reasons
    gpioWrite(LINE_IN, 0);
}

void prepareExit() {
    gpioSetMode(LINE_OUT, PI_OUTPUT);
    gpioSetMode(LINE_IN, PI_OUTPUT);
    gpioSetMode(START_MEASUREMENT_BUTTON, PI_OUTPUT);
    gpioSetMode(CALIBRATION_MODE_BUTTON, PI_OUTPUT);
    gpioSetMode(LINE_OUT_MODE_BUTTON, PI_OUTPUT);
    gpioSetMode(USB_OUT_MODE_BUTTON, PI_OUTPUT);
    gpioSetMode(HDMI_OUT_MODE_BUTTON, PI_OUTPUT);
    gpioSetMode(EXIT_BUTTON, PI_OUTPUT);

    gpioWrite(LINE_OUT, 0);
    gpioWrite(LINE_IN, 0);
    gpioWrite(START_MEASUREMENT_BUTTON, 0);
    gpioWrite(CALIBRATION_MODE_BUTTON, 0);
    gpioWrite(LINE_OUT_MODE_BUTTON, 0);
    gpioWrite(USB_OUT_MODE_BUTTON, 0);
    gpioWrite(HDMI_OUT_MODE_BUTTON, 0);
    gpioWrite(EXIT_BUTTON, 0);
    gpioWrite(START_MEASUREMENT_LED, 0);
    gpioWrite(CALIBRATION_MODE_GREEN_LED, 0);
    gpioWrite(CALIBRATION_MODE_YELLOW_LED, 0);
    gpioWrite(CALIBRATION_MODE_RED_LED, 0);
    gpioWrite(LINE_OUT_MODE_LED, 0);
    gpioWrite(USB_OUT_MODE_LED, 0);
    gpioWrite(HDMI_OUT_MODE_LED, 0);
    gpioWrite(EXIT_LED, 0);
    
    // Terminate library
    gpioTerminate();
    
    printf("\nExit\n");
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

    for (int i = 0; i < iterations; i++) {
        printf("### Measurement %d\n", i);
        // Open PCM device for playback. 
        // Measurement is only working consistently if pcm device is opened and closed in every iteration.
        if (measurementMode == USB_OUT_MODE_BUTTON) {
            status = snd_pcm_open(&handle, ALSA_USB_TOP_OUT, SND_PCM_STREAM_PLAYBACK, 0);
            if (status < 0) {
                // Unable to open pcm device
                status = snd_pcm_open(&handle, ALSA_USB_BOTTOM_OUT, SND_PCM_STREAM_PLAYBACK, 0);
                if (status < 0) {
                    // Unable to open pcm device
                    status = snd_pcm_open(&handle, ALSA_USB_TOP2_OUT, SND_PCM_STREAM_PLAYBACK, 0);
                    if (status < 0) {
                        // Unable to open pcm device
                        status = snd_pcm_open(&handle, ALSA_USB_BOTTOM2_OUT, SND_PCM_STREAM_PLAYBACK, 0);
                        if (status < 0) {
                            printf("audio_lag_module.c l.417: Unable to open PCM Device\n");
                            return;
                        }
                    }
                }
            }
        }
        // HDMI_MODE
        else {
            status = snd_pcm_open(&handle, ALSA_HDMI_OUT, SND_PCM_STREAM_PLAYBACK, 0);
            if (status < 0) {
                printf("audio_lag_module.c l.428: Unable to open PCM Device\n");
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
            printf("audio_lag_module.c l.462: Unable to set PCM devices hardware parameters\n");
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
                printf("audio_lag_module.c l.493: Underrun occured during snd_pcm_writei -> Preparing PCM device to continue measurement\n");
                snd_pcm_prepare(handle);
            }
            else if (status < 0) {
                printf("audio_lag_module.c l.497: Error during snd_pcm_writei -> Reopening PCM device\n");
                break;
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
        snd_pcm_drain(handle);
        snd_pcm_close(handle);
        free(buffer);
        time_sleep(signalIntervalInS);
    }
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
    gpioWrite(EXIT_LED, 0);
}

void waitForUserInput() {
    while (1) {
        if (gpioRead(START_MEASUREMENT_BUTTON) == 1) {
            gpioWrite(START_MEASUREMENT_LED, 1);
            resetMeasurement();
            if (measurementMode == LINE_OUT_MODE_BUTTON) {
                startMeasurementLineOut(MEASURE);
            }
            // USB_, HDMI_, PCIE_OUT
            else {
                startMeasurementDigitalOut(MEASURE);
            }
            writeMeasurementsToCSV();
            gpioWrite(START_MEASUREMENT_LED, 0);
        }
        else if (gpioRead(CALIBRATION_MODE_BUTTON) == 1) {
            while (gpioRead(START_MEASUREMENT_BUTTON) == 0
                    && gpioRead(LINE_OUT_MODE_BUTTON) == 0
                    && gpioRead(USB_OUT_MODE_BUTTON) == 0
                    && gpioRead(HDMI_OUT_MODE_BUTTON) == 0
                    && gpioRead(EXIT_BUTTON) == 0) {
                resetMeasurement();
                if (measurementMode == LINE_OUT_MODE_BUTTON) {
                    startMeasurementLineOut(CALIBRATE);
                }
                // USB_, HDMI_, PCIE_OUT
                else {
                    startMeasurementDigitalOut(CALIBRATE);
                }
                if (validMeasurementsCount == TOTAL_CALIBRATION_MEASUREMENTS) {
                    userFeedbackGoodSignal();
                }
                else if (validMeasurementsCount > TOTAL_CALIBRATION_MEASUREMENTS / 5) {
                    userFeedbackMediumSignal();
                }
                else {
                    userFeedbackBadSignal();
                }
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
        else if (gpioRead(EXIT_BUTTON) == 1) {
            gpioWrite(EXIT_LED, 1);
            time_sleep(0.1);
            prepareExit();
            return;
        }
        else {
            // No action, just keeping the while loop going
        }
    }
}

int main(void) {

    initGPIOs();
    waitForUserInput();
}

#include <pigpio.h>
#include <stdio.h>

// Constants
#define LINE_OUT 17
#define LINE_IN 27
#define TOTAL_MEASUREMENTS 10
#define SIGNAL_FREQUENCY_IN_HZ 200.0
#define HALF_WAVELENGTH_IN_S (1.0 / SIGNAL_FREQUENCY_IN_HZ / 2.0)

// v1.2: Iterative measurement, condition = measurement frequency > latency of DUT
#define MEASUREMENT_FREQUENCY_IN_S 1.0

// v1.2.1: Iterative measurement, condition = measurement frequency > latency of DUT only for calibration measurements
#define TOTAL_CALIBRATION_MEASUREMENTS 100
#define MEASUREMENT_FREQUENCY_MARGIN_IN_SECONDS 0.01

// v2.1: Recursive measurement, condition = no noise through signal loss/add
// v2.2: TODO: Recursive measurement, condition = no noise through signal add while signal loss is recognized by a timeout
#define RUNNING 0
#define FINISHED 1


// Variables

// v1.1: Iterative measurement, condition = no noise through signal loss/add
uint32_t startTicks[TOTAL_MEASUREMENTS];
uint32_t endTicks[TOTAL_MEASUREMENTS];
int outCount = 0;
int inCount = 0;

// v1.2: Iterative measurement, condition = measurement frequency > latency of DUT
uint32_t startTick, endTick;

int latencyInMicros;
int validMeasurmentsCount = 0;
int latencyMeasurementsInMicros[TOTAL_MEASUREMENTS];
int maxLatencyInMicros = -1;
int minLatencyInMicros = -1;
int sumOfLatenciesInMicros = 0;
int avgLatencyInMicros;
int status;

// v2.1: Recursive measurement, condition = no noise through signal loss/add
// v2.2: TODO: Recursive measurement, condition = no noise through signal add while signal loss is recognized by a timeout
int measurementStatus;


// Line-out callback
void onLineOut(int gpio, int level, uint32_t tick) {
    printf("GPIO %d state changed to level %d at %d\n", gpio, level, tick);
    
    if (level == 1) {
        
        // v1.1: Iterative measurement, condition = no noise through signal loss/add
        startTicks[outCount] = tick;
        
        // v1.2: Iterative measurement, condition = measurement frequency > latency of DUT
        //startTick = tick;
        
        outCount += 1;
    }
}

// Line-in callback
void onLineIn(int gpio, int level, uint32_t tick) {
    printf("GPIO %d state changed to level %d at %d\n", gpio, level, tick);
    
    if (level == 1) {
        
        // v1.1: Iterative measurement, condition = no noise through signal loss/add
        endTicks[inCount] = tick;
        
        // v1.2: Iterative measurement, condition = measurement frequency > latency of DUT
        //endTick = tick;
        
        // Calculating latency in micros
        // v1.1: Iterative measurement, condition = no noise through signal loss/add
        latencyInMicros = endTicks[inCount] - startTicks[inCount];
        
        // v1.2: Iterative measurement, condition = measurement frequency > latency of DUT
        //latencyInMicros = endTick - startTick;
        
        // The uint32_t tick parameter represents the number of microseconds since boot.
        // This wraps around from 4294967295 to 0 roughly every 72 minutes.
        // Thats why the provided latency could be both negative and wrong in this specific situation.
        if (latencyInMicros >= 0) {
            validMeasurmentsCount += 1;
            
            // Saving valid measurement
            latencyMeasurementsInMicros[inCount] = latencyInMicros;
            printf("The signal had a latency of %d microseconds\n", latencyInMicros);
            
            // Calculating running descriptive values (min, max, avg)
            if (maxLatencyInMicros == -1) {
                maxLatencyInMicros = latencyInMicros;
            }
            else if (latencyInMicros > maxLatencyInMicros) {
                maxLatencyInMicros = latencyInMicros;
            }
            else {}
            
            if (minLatencyInMicros == -1) {
                minLatencyInMicros = latencyInMicros;
            }
            else if (latencyInMicros >= 0 && latencyInMicros < minLatencyInMicros) {
                minLatencyInMicros = latencyInMicros;
            }
            else {}
            
            sumOfLatenciesInMicros += latencyInMicros;
            
            avgLatencyInMicros = sumOfLatenciesInMicros / validMeasurmentsCount;
        }
        inCount += 1;
    }
    else {
        
        /* v2.1: Recursive measurement, condition = no noise through signal loss/add
        // v2.2: TODO: Recursive measurement, condition = no noise through signal add while signal loss is recognized by a timeout
        
        // Start next measurement or finish
        if (inCount < TOTAL_MEASUREMENTS) {
            // Send signal through output (3.3V square wave, measurement frequency = latency of DUT)
            printf("\n\n----- Measurement %d started -----\n", outCount + 1);
            status = gpioWrite(LINE_OUT, 1);
            printf("status (0 = OK; <0 = ERROR): %d\n", status);
            time_sleep(HALF_WAVELENGTH_IN_S);
            status = gpioWrite(LINE_OUT, 0);
            printf("status (0 = OK; <0 = ERROR): %d\n", status);
        }
        else {
            measurementStatus = FINISHED;
        }
        */
    }
}

int main(void) {
    int status/*, cfg*/;
    
    gpioTerminate();
    /* Documentation (https://abyz.me.uk/rpi/pigpio/cif.html):
    "If you intend to rely on signals sent to your application,
    you should turn off the internal signal handling [...]:"
    
    cfg = gpioCfgGetInternals();
    cfg |= PI_CFG_NOSIGHANDLER;  // (1<<10)
    gpioCfgSetInternals(cfg);*/

    // Initialize library
    status = gpioInitialise();
    //printf("Status after gpioInitialise: %d\n", status);

    // Set GPIO Modes
    gpioSetMode(LINE_OUT, PI_OUTPUT);
    gpioSetMode(LINE_IN, PI_INPUT);

    // Register GPIO state change callback
    gpioSetAlertFunc(LINE_OUT, onLineOut);
    gpioSetAlertFunc(LINE_IN, onLineIn);
    

    // Iterative measurement
    
    for (int i = 0; i < TOTAL_MEASUREMENTS; i++) {
        
        
        /* v1.2.1: Iterative measurement, condition = measurement frequency > latency of DUT only for calibration measurements
        
        if (i == TOTAL_CALIBRATION_MEASUREMENTS) {
            measurementFrequencyInSeconds = maxLatencyInMicros / 1000000 + MEASUREMENT_FREQUENCY_MARGIN_IN_SECONDS;
        }
        */
        
        
        /* v1.1: Iterative measurement, condition = no noise through signal loss/add
        
        // Send signal through output (3.3V square wave, measurement frequency = SIGNAL_FREQUENCY_IN_HZ)
        printf("\n\n----- Measurement %d started -----\n", i + 1);
        status = gpioWrite(LINE_OUT, 1);
        printf("status (0 = OK; <0 = ERROR): %d\n", status);
        time_sleep(HALF_WAVELENGTH_IN_S);
        status = gpioWrite(LINE_OUT, 0);
        printf("status (0 = OK; <0 = ERROR): %d\n", status);
        time_sleep(HALF_WAVELENGTH_IN_S);
        */
        
        // v3: Bitstream measurement: Test Signal (110 | 100 | 10); bitFrequency = HALF_WAVELENGTH_IN_S
        printf("\n\n----- Measurement %d started -----\n", i + 1);
        // 110
        status = gpioWrite(LINE_OUT, 1);
        time_sleep(HALF_WAVELENGTH_IN_S * 2.0);
        status = gpioWrite(LINE_OUT, 0);
        time_sleep(HALF_WAVELENGTH_IN_S);
        // bitstream end signal
        status = gpioWrite(LINE_OUT, 0);
        time_sleep(HALF_WAVELENGTH_IN_S * 0.5);
        status = gpioWrite(LINE_OUT, 1);
        time_sleep(HALF_WAVELENGTH_IN_S * 0.5);
        status = gpioWrite(LINE_OUT, 0);
        time_sleep(HALF_WAVELENGTH_IN_S * 0.5);
        // 101
        status = gpioWrite(LINE_OUT, 1);
        time_sleep(HALF_WAVELENGTH_IN_S);
        status = gpioWrite(LINE_OUT, 0);
        time_sleep(HALF_WAVELENGTH_IN_S);
        status = gpioWrite(LINE_OUT, 1);
        time_sleep(HALF_WAVELENGTH_IN_S);
        // bitstream end signal
        status = gpioWrite(LINE_OUT, 0);
        time_sleep(HALF_WAVELENGTH_IN_S * 0.5);
        status = gpioWrite(LINE_OUT, 1);
        time_sleep(HALF_WAVELENGTH_IN_S * 0.5);
        status = gpioWrite(LINE_OUT, 0);
        time_sleep(HALF_WAVELENGTH_IN_S * 0.5);
        
        
        // v1.2: Iterative measurement, condition = measurement frequency > latency of DUT
        // v1.2.1: Iterative measurement, condition = measurement frequency > latency of DUT only for calibration measurements
        
        // Measurement frequency = MEASUREMENT_FREQUENCY_IN_S
        //time_sleep(MEASUREMENT_FREQUENCY_IN_S);
    }
    printf("\n%d\n", status);
    
    /* v2.1: Recursive measurement, condition = no noise through signal loss/add
    // v2.2: TODO: Recursive measurement, condition = no noise through signal add while signal loss is recognized by a timeout
    
    // Send signal through output (3.3V square wave, measurement frequency = latency of DUT)
    measurementStatus = RUNNING;
    printf("\n\n----- Measurement %d started -----\n", outCount + 1);
    status = gpioWrite(LINE_OUT, 1);
    printf("status (0 = OK; <0 = ERROR): %d\n", status);
    time_sleep(HALF_WAVELENGTH_IN_S);
    status = gpioWrite(LINE_OUT, 0);
    printf("status (0 = OK; <0 = ERROR): %d\n", status);
    
    // Waiting for measurement series to finish
    while (measurementStatus == RUNNING) {
        time_sleep(1);
    }
    */
    
    // Print measurements
    for (int i = 0; i < TOTAL_MEASUREMENTS; i++) {
        
        printf("\n##### Measurement %d latency: %d\n", i + 1, latencyMeasurementsInMicros[i]);
    }
    
    gpioTerminate();
    
    // TODO: Saving measurements to .csv format
    
    printf("\nExit\n");
}

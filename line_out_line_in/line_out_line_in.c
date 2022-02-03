#include <pigpio.h>

int main(void) {
    const int OUTPUT_GPIO_17 = 17;
    const int INPUT_GPIO_27 = 27;

    uint32_t startTick, endTick;
    int latencyInMicros;
    int measurementCount = 0;
    int latencyMeasurementsInMicros[10000];

    /* Documentation (https://abyz.me.uk/rpi/pigpio/cif.html):
    "If you intend to rely on signals sent to your application,
    you should turn off the internal signal handling [...]:"
    */
    int cfg = gpioCfgGetInternals();
    cfg |= PI_CFG_NOSIGHANDLER;  // (1<<10)
    gpioCfgSetInternals(cfg);

    // Initialize library
    gpioInitialise();

    // Set GPIO Modes
    gpioSetMode(OUTPUT_GPIO_17, PI_OUTPUT);
    gpioSetMode(INPUT_GPIO_27, PI_INPUT);

    // Register GPIO state change callback
    gpioSetAlertFunc(OUTPUT_GPIO_17, onStateChanged);
    gpioSetAlertFunc(INPUT_GPIO_27, onStateChanged);

    // Send signal through output (3.3V square wave)
    gpioWrite(OUTPUT_GPIO_17, 1);
    gpioWrite(OUTPUT_GPIO_17, 0);
}

// GPIO state changed
void onStateChanged(int gpio, int level, uint32_t tick) {
    if (level == 1) {
        if (gpio == OUTPUT_GPIO_17) {
            startTick = tick;
        }
        if (gpio == INPUT_GPIO_27) {
            endTick = tick;
            latencyInMicros = endTick - startTick;
            // The uint32_t tick parameter represents the number of microseconds since boot.
            // This wraps around from 4294967295 to 0 roughly every 72 minutes.
            // Thats why the provided latency could be both negative and wrong in this specific situation
            if (latencyInMicros >= 0) {
                latencyMeasurementsInMicros[measurementCount] = latencyInMicros;
                printf("The signal had a latency of %d microseconds", latencyInMicros);
                measurementCount += 1;
            }
        }
    }
}
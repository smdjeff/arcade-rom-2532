#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "portable.h"


int gpioInitialise(void) { 
    printf("gpioInitialise\n");
    sleep(1);
    return 0;
}

void gpioTerminate(void) {
    printf("gpioTerminate\n");
    sleep(1);
}

int gpioSetMode(unsigned gpio, unsigned mode) { 
    printf("gpio[%d] %s\n", gpio, (mode?"input":"output"));
    return 0; 
}

int gpioSetTimerFunc(unsigned timer, unsigned millis, gpioTimerFunc_t f) {
    return 0; 
}

int gpioSetAlertFunc(unsigned user_gpio, gpioAlertFunc_t f) {
    return 0; 
}

void* gpioStartThread(gpioThreadFunc_t f, void *userdata) {
    return 0;
}

static int gpioState[41] = {0,};

int gpioWrite(unsigned gpio, unsigned level) {
    printf("gpio[%d] wr %d\n", gpio, level);
//    sleep(1);
    gpioState[gpio] = level;
    return 0; 
}

int gpioRead(unsigned gpio) { 
    int level = gpioState[gpio];
    printf("gpio[%d] rd %d\n", gpio, level);
//    sleep(1);
    return level;
}

int gpioSetPullUpDown(unsigned gpio, unsigned pud) { 
    return 0; 
}

int gpioPWM(unsigned user_gpio, unsigned dutycycle) { 
    return 0; 
}

int gpioSetPWMfrequency(unsigned gpio, unsigned frequency) {
    return 0;
}

int gpioGetPWMfrequency(unsigned gpio) {
    return 0;
}

int gpioCfgClock(unsigned micros, unsigned peripheral, unsigned source) {
    return 0;
}

int gpioGlitchFilter(unsigned user_gpio, unsigned steady) {
    return 0;
}

int i2cWriteByteData(unsigned handle, unsigned i2cReg, unsigned bVal) {
    return 0; 
}

int i2cOpen(unsigned i2cBus, unsigned i2cAddr, unsigned i2cFlags) { 
    return 0; 
}


#ifndef _PORTABLE_H_
#define _PORTABLE_H_

//////////////////////////////////////////////////////////////////////////
// arbitrary connections to raspberry pi io
    
#define IO_D0                    17
#define IO_D1                    18
#define IO_D2                    27
#define IO_D3                    22
#define IO_D4                    23
#define IO_D5                    24
#define IO_D6                    25
#define IO_D7                    4

#define IO_S21                   5
#define IO_S18                   6
#define IO_S20A                  12
#define IO_S20B                  13

#define EN_VPP                   16
#define EN_VCC                   26
#define EN_NOE                   20
#define EN_NCE                   21

#define SPI_MOSI                 10
#define D_DIR                    9
#define SPI_CLK                  11
#define SPI_CE0                  8
#define AD_NOE                   7


// https://github.com/joan2937/pigpio/issues/397
#define gpioCancelTimer(t) gpioSetTimerFunc(t,PI_MIN_MS,0)

#define TIMER_0                  0
#define TIMER_1                  1
#define TIMER_2                  2
#define TIMER_3                  3
#define TIMER_4                  4
#define TIMER_5                  5
#define TIMER_6                  6
#define TIMER_7                  7
#define TIMER_8                  8
#define TIMER_9                  9
    

    
///////////////////////////////////////////////////////////////////////////
// poorman's hardware abstraction layer 
// allowing this code to be built on a desktop
// basically just a pigpio lib that debug outputs to stdio
// rather than real hardware

#ifdef RASPBERRY
  
  #include <pigpio.h> // http://abyz.me.uk/rpi/pigpio/
  
#else

#define PI_MIN_MS    0
#define PI_INPUT     0
#define PI_OUTPUT    1
#define PI_PUD_OFF   0
#define PI_PUD_DOWN  1
#define PI_PUD_UP    2
#define RISING_EDGE  0
#define FALLING_EDGE 1
    

typedef void (*gpioTimerFunc_t)    (void);
typedef void (*gpioAlertFunc_t)    (int gpio, int level, uint32_t tick);
typedef void *(gpioThreadFunc_t)   (void*);

int gpioCfgClock(unsigned micros, unsigned peripheral, unsigned source);
int gpioInitialise(void);
void gpioTerminate(void);
void* gpioStartThread(gpioThreadFunc_t f, void *userdata);
int gpioSetMode(unsigned gpio, unsigned mode);
int gpioSetTimerFunc(unsigned timer, unsigned millis, gpioTimerFunc_t f);
int gpioSetAlertFunc(unsigned user_gpio, gpioAlertFunc_t f);
int gpioGlitchFilter(unsigned user_gpio, unsigned steady);
int gpioWrite(unsigned gpio, unsigned level);
int gpioRead(unsigned gpio);
int gpioSetPullUpDown(unsigned gpio, unsigned pud);
int gpioPWM(unsigned user_gpio, unsigned dutycycle);
int gpioSetPWMfrequency(unsigned gpio, unsigned frequency);
int gpioGetPWMfrequency(unsigned gpio);
int i2cWriteByteData(unsigned handle, unsigned i2cReg, unsigned bVal);
int i2cOpen(unsigned i2cBus, unsigned i2cAddr, unsigned i2cFlags);

#endif

#endif // _PORTABLE_H_


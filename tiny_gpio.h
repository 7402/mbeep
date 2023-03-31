/*
   tiny_gpio.h
   2023-03-29
   Public Domain
*/

#ifndef tiny_gpio_H
#define tiny_gpio_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

/* gpio modes. */

#define PI_INPUT  0
#define PI_OUTPUT 1
#define PI_ALT0   4
#define PI_ALT1   5
#define PI_ALT2   6
#define PI_ALT3   7
#define PI_ALT4   3
#define PI_ALT5   2

void gpioSetMode(unsigned gpio, unsigned mode);
int gpioGetMode(unsigned gpio);

#define PI_PUD_OFF  0
#define PI_PUD_DOWN 1
#define PI_PUD_UP   2
void gpioSetPullUpDown(unsigned gpio, unsigned pud);

int gpioRead(unsigned gpio);
void gpioWrite(unsigned gpio, unsigned level);
void gpioTrigger(unsigned gpio, unsigned pulseLen, unsigned level);
uint32_t gpioReadBank1(void);
uint32_t gpioReadBank2(void);
void gpioClearBank1(uint32_t bits);
void gpioClearBank2(uint32_t bits);
void gpioSetBank1(uint32_t bits);
void gpioSetBank2(uint32_t bits);
unsigned gpioHardwareRevision(void);
int gpioInitialise(void);

#ifdef __cplusplus
}
#endif

#endif
#ifndef LED_H
#define LED_H

#include "arch/xtensa_lx6/drivers/esp32_gpio/gpio.h"

#define LED_PIN 2

void led_init(void);
void led_on(void);
void led_off(void);
void led_toggle(void);

#endif /* LED_H */

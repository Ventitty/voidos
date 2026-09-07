#include "src/drivers/esp32_led/led.h"

static int led_state = 0;

void led_init(void) {
    gpio_set_mode(LED_PIN, GPIO_MODE_OUTPUT, GPIO_PULL_NONE);
    led_state = 0;
}

void led_on(void) {
    led_state = 1;
    gpio_write(LED_PIN, led_state);
}

void led_off(void) {
    led_state = 0;
    gpio_write(LED_PIN, led_state);
}

void led_toggle(void) {
    led_state = !led_state;
    gpio_write(LED_PIN, led_state);
}

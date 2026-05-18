#pragma once

#include <stdint.h>

typedef enum {
    GPIO_PIN_RESET = 0,
    GPIO_PIN_SET   = 1,
} gpio_pin_state_t;

void gpio_led_init(void);
void gpio_led_set(gpio_pin_state_t state);
void gpio_led_toggle(void);

/* Available in mock implementation only — for use in unit tests */
gpio_pin_state_t gpio_led_get_state(void);

#include "hal/gpio.h"
#include <stdio.h>

static gpio_pin_state_t led_state = GPIO_PIN_RESET;

void gpio_led_init(void)
{
    led_state = GPIO_PIN_RESET;
}

void gpio_led_set(gpio_pin_state_t state)
{
    led_state = state;
}

void gpio_led_toggle(void)
{
    led_state = (led_state == GPIO_PIN_SET) ? GPIO_PIN_RESET : GPIO_PIN_SET;
}

gpio_pin_state_t gpio_led_get_state(void)
{
    return led_state;
}

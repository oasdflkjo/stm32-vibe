#include "hal/gpio.h"
#include "stm32l1xx.h"

#define LED_PIN 5U

static uint32_t led_state = 0;

void gpio_led_init(void)
{
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    (void)RCC->AHBENR;

    GPIOA->MODER &= ~GPIO_MODER_MODER5_Msk;
    GPIOA->MODER |= GPIO_MODER_MODER5_0;
    GPIOA->OTYPER &= ~(1U << LED_PIN);
    GPIOA->PUPDR &= ~GPIO_PUPDR_PUPDR5_Msk;
}

void gpio_led_set(gpio_pin_state_t state)
{
    if (state == GPIO_PIN_SET) {
        GPIOA->BSRR = GPIO_BSRR_BS_5;
        led_state = 1;
    } else {
        GPIOA->BSRR = GPIO_BSRR_BR_5;
        led_state = 0;
    }
}

void gpio_led_toggle(void)
{
    gpio_led_set(led_state ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

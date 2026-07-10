#include "led_task.h"
#include "hal/gpio.h"
#include "hal/systick.h"
#include <stdint.h>

#define BLINK_PERIOD_MS 500U

void led_task_init(void)
{
    gpio_led_init();
    systick_init();
}

void led_task_run(void)
{
    gpio_led_toggle();
    systick_delay_ms(BLINK_PERIOD_MS);
}

void led_task_run_with_idle(led_task_idle_fn_t idle)
{
    gpio_led_toggle();
    for (uint32_t elapsed_ms = 0U; elapsed_ms < BLINK_PERIOD_MS;
         elapsed_ms++) {
        if (idle != 0) {
            idle();
        }
        systick_delay_ms(1U);
    }
}

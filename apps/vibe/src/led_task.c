#include "led_task.h"
#include "hal/gpio.h"
#include "hal/systick.h"

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

#include "led_task.h"

#define BLINK_PERIOD_MS 500U

static led_task_output_fn_t set_output;
static uint32_t next_toggle_ms;
static bool led_enabled;

void led_task_init(led_task_output_fn_t output, uint32_t now_ms)
{
    set_output = output;
    led_enabled = false;
    next_toggle_ms = now_ms + BLINK_PERIOD_MS;
    if (set_output != 0) {
        set_output(false);
    }
}

void led_task_process(uint32_t now_ms)
{
    if ((int32_t)(now_ms - next_toggle_ms) >= 0) {
        led_enabled = !led_enabled;
        next_toggle_ms += BLINK_PERIOD_MS;
        if (set_output != 0) {
            set_output(led_enabled);
        }
    }
}

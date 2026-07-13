#include "hal/systick.h"

static uint32_t milliseconds;

void systick_init(void) { milliseconds = 0U; }

uint32_t systick_now_ms(void) { return milliseconds; }

void systick_delay_ms(uint32_t ms)
{
    milliseconds += ms;
}

#pragma once

#include <stdint.h>

void systick_init(void);
uint32_t systick_now_ms(void);
void systick_delay_ms(uint32_t ms);

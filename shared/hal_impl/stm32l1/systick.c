#include "hal/systick.h"
#include "stm32l1xx.h"

void systick_init(void)
{
    SysTick_Config(SystemCoreClock / 1000U);
}

void systick_delay_ms(uint32_t ms)
{
    /* Simple busy-wait using SysTick — replace with interrupt-driven if needed */
    for (volatile uint32_t i = 0; i < ms * (SystemCoreClock / 1000U / 4U); i++) {
        __NOP();
    }
}

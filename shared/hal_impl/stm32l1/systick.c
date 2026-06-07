#include "hal/systick.h"
#include "stm32l1xx.h"

void systick_init(void)
{
    SysTick->LOAD = (SystemCoreClock / 1000U) - 1U;
    SysTick->VAL = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
}

void systick_delay_ms(uint32_t ms)
{
    /* Simple busy-wait using SysTick — replace with interrupt-driven if needed */
    for (volatile uint32_t i = 0; i < ms * (SystemCoreClock / 1000U / 4U); i++) {
        __NOP();
    }
}

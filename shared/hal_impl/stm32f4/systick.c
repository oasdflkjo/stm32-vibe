#include "hal/systick.h"
#include "platform/device.h"

static volatile uint32_t milliseconds;

void SysTick_Handler(void)
{
    milliseconds++;
}

void systick_init(void)
{
    milliseconds = 0U;
    SysTick->LOAD = (SystemCoreClock / 1000U) - 1U;
    SysTick->VAL = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk |
                    SysTick_CTRL_ENABLE_Msk;
}

uint32_t systick_now_ms(void)
{
    return milliseconds;
}

void systick_delay_ms(uint32_t ms)
{
    while (ms > 0U) {
        SysTick->VAL = 0U;
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0U) {
        }
        ms--;
    }
}

#include "hal/critical.h"
#include "stm32l1xx.h"

uint32_t critical_section_enter(void)
{
    uint32_t state = __get_PRIMASK();
    __disable_irq();
    return state;
}

void critical_section_exit(uint32_t state)
{
    __set_PRIMASK(state);
}

#include "hal/critical.h"

uint32_t critical_section_enter(void)
{
    return 0U;
}

void critical_section_exit(uint32_t state)
{
    (void)state;
}

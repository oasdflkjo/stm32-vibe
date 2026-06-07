#pragma once

#include <stdint.h>

uint32_t critical_section_enter(void);
void critical_section_exit(uint32_t state);

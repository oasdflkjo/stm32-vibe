#pragma once

#include <stdint.h>

int watchdog_init(uint32_t timeout_ms);
void watchdog_refresh(void);

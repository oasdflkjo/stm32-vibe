#pragma once

#include <stdint.h>

/*
 * ITM / SWO trace output.
 *
 * Call itm_init() once at startup.  Use itm_puts() / itm_putchar() directly —
 * no printf / newlib stdio involved so there is no heap requirement.
 *
 * swo_baud must match what the debugger (OpenOCD / ST-Link) is configured
 * to receive.  A safe default for STM32L1 at 2 MHz is 72000.
 */

void itm_init(uint32_t cpu_hz, uint32_t swo_baud);
void itm_putchar(uint8_t ch);
void itm_puts(const char *s);

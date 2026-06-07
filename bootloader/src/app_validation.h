#pragma once

#include <stdint.h>

#define APP_START_ADDR 0x08004000U
#define APP_END_ADDR   0x08080000U
#define RAM_START_ADDR 0x20000000U
#define RAM_END_ADDR   0x20014000U

int app_vectors_are_valid(uint32_t stack_pointer, uint32_t reset_handler);

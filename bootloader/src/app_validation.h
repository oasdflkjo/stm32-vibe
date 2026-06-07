#pragma once

#include "image/app_manifest.h"
#include <stdint.h>

#define APP_START_ADDR APP_IMAGE_START_ADDR
#define APP_END_ADDR   APP_IMAGE_END_ADDR
#define RAM_START_ADDR 0x20000000U
#define RAM_END_ADDR   0x20014000U

int app_vectors_are_valid(uint32_t stack_pointer, uint32_t reset_handler);

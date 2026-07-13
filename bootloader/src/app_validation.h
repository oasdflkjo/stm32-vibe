#pragma once

#include "image/app_manifest.h"
#include "boot/boot_runtime.h"
#include <stdint.h>

#define APP_START_ADDR APP_IMAGE_START_ADDR
#define APP_END_ADDR   APP_IMAGE_END_ADDR
#define RAM_START_ADDR 0x20000000U
#if defined(STM32F446xx)
#define RAM_END_ADDR   0x20020000U
#else
#define RAM_END_ADDR   0x20014000U
#endif

int app_vectors_are_valid(uint32_t stack_pointer, uint32_t reset_handler);
int app_vectors_are_valid_for_slot(uint32_t stack_pointer,
                                   uint32_t reset_handler,
                                   uint32_t slot_start,
                                   uint32_t slot_end);
int app_relative_vectors_are_valid(uint32_t stack_pointer,
                                   uint32_t reset_offset,
                                   uint32_t image_size);

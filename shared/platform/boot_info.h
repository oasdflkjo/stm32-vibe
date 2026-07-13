#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "boot/boot_runtime.h"

typedef struct {
    uint32_t image_base;
    uint32_t image_size;
    uint32_t slot;
    uint32_t boot_attempt;
    bool pending;
} platform_boot_info_t;

bool platform_boot_info_init(void);
bool platform_boot_info_init_from(const boot_handoff_t *handoff);
bool platform_boot_info_get(platform_boot_info_t *info);

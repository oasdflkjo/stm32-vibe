#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum {
    BOOT_FLASH_OK = 0,
    BOOT_FLASH_INVALID_ARGUMENT = 1,
    BOOT_FLASH_ERROR = 2,
} boot_flash_result_t;

boot_flash_result_t boot_flash_erase_slot_b(uint32_t image_size);
boot_flash_result_t boot_flash_write_slot_b(uint32_t offset,
                                            const uint8_t *data,
                                            size_t len);
const uint8_t *boot_flash_slot_b_base(void);

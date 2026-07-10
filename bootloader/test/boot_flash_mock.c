#include "boot_flash.h"
#include "image/flash_layout.h"
#include <string.h>

static uint8_t slot_b[APP_SLOT_SIZE];
static boot_flash_result_t forced_result;

void boot_flash_mock_reset(void)
{
    memset(slot_b, 0xFF, sizeof(slot_b));
    forced_result = BOOT_FLASH_OK;
}

void boot_flash_mock_set_result(boot_flash_result_t result)
{
    forced_result = result;
}

boot_flash_result_t boot_flash_erase_slot_b(uint32_t image_size)
{
    if (forced_result != BOOT_FLASH_OK) {
        return forced_result;
    }

    if ((image_size == 0U) || (image_size > APP_SLOT_SIZE)) {
        return BOOT_FLASH_INVALID_ARGUMENT;
    }

    memset(slot_b, 0xFF, sizeof(slot_b));
    return BOOT_FLASH_OK;
}

boot_flash_result_t boot_flash_write_slot_b(uint32_t offset,
                                            const uint8_t *data,
                                            size_t len)
{
    if (forced_result != BOOT_FLASH_OK) {
        return forced_result;
    }

    if (((len != 0U) && (data == 0)) || (offset > APP_SLOT_SIZE) ||
        (len > (APP_SLOT_SIZE - offset))) {
        return BOOT_FLASH_INVALID_ARGUMENT;
    }

    memcpy(&slot_b[offset], data, len);
    return BOOT_FLASH_OK;
}

const uint8_t *boot_flash_slot_b_base(void)
{
    return slot_b;
}

#include "boot_flash.h"
#include "boot/boot_state.h"
#include "image/flash_layout.h"
#include <string.h>

static uint8_t slot_a[APP_SLOT_SIZE];
static uint8_t slot_b[APP_SLOT_SIZE];
static boot_flash_result_t forced_result;

static int slot_is_valid(uint32_t slot)
{
    return (slot == BOOT_SLOT_A) || (slot == BOOT_SLOT_B);
}

static uint8_t *slot_storage(uint32_t slot)
{
    return slot == BOOT_SLOT_A ? slot_a : slot_b;
}

void boot_flash_mock_reset(void)
{
    memset(slot_a, 0xFF, sizeof(slot_a));
    memset(slot_b, 0xFF, sizeof(slot_b));
    forced_result = BOOT_FLASH_OK;
}

void boot_flash_mock_set_result(boot_flash_result_t result)
{
    forced_result = result;
}

boot_flash_result_t boot_flash_erase_slot(uint32_t slot, uint32_t image_size)
{
    if (forced_result != BOOT_FLASH_OK) {
        return forced_result;
    }

    if (!slot_is_valid(slot)) {
        return BOOT_FLASH_INVALID_ARGUMENT;
    }

    if ((image_size == 0U) || (image_size > APP_SLOT_SIZE)) {
        return BOOT_FLASH_INVALID_ARGUMENT;
    }

    memset(slot_storage(slot), 0xFF, APP_SLOT_SIZE);
    return BOOT_FLASH_OK;
}

boot_flash_result_t boot_flash_write_slot(uint32_t slot,
                                          uint32_t offset,
                                          const uint8_t *data,
                                          size_t len)
{
    if (forced_result != BOOT_FLASH_OK) {
        return forced_result;
    }

    if (!slot_is_valid(slot)) {
        return BOOT_FLASH_INVALID_ARGUMENT;
    }

    if (((len != 0U) && (data == 0)) || (offset > APP_SLOT_SIZE) ||
        (len > (APP_SLOT_SIZE - offset))) {
        return BOOT_FLASH_INVALID_ARGUMENT;
    }

    memcpy(&slot_storage(slot)[offset], data, len);
    return BOOT_FLASH_OK;
}

const uint8_t *boot_flash_slot_base(uint32_t slot)
{
    if (!slot_is_valid(slot)) {
        return 0;
    }

    return slot_storage(slot);
}

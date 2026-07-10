#include "boot_flash.h"
#include "boot/boot_state.h"
#include "image/flash_layout.h"
#include "stm32l1xx.h"

#define BOOT_FLASH_PAGE_SIZE 256U
#define FLASH_PEKEY1 0x89ABCDEFU
#define FLASH_PEKEY2 0x02030405U
#define FLASH_PRGKEY1 0x8C9DAEBFU
#define FLASH_PRGKEY2 0x13141516U
#define FLASH_ERROR_FLAGS \
    (FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_SIZERR | \
     FLASH_SR_OPTVERR | FLASH_SR_OPTVERRUSR)

static boot_flash_result_t wait_ready(void)
{
    while ((FLASH->SR & FLASH_SR_BSY) != 0U) {
    }

    if ((FLASH->SR & FLASH_ERROR_FLAGS) != 0U) {
        FLASH->SR = FLASH_ERROR_FLAGS;
        return BOOT_FLASH_ERROR;
    }

    if ((FLASH->SR & FLASH_SR_EOP) != 0U) {
        FLASH->SR = FLASH_SR_EOP;
    }

    return BOOT_FLASH_OK;
}

static boot_flash_result_t unlock_flash(void)
{
    if ((FLASH->PECR & FLASH_PECR_PELOCK) != 0U) {
        FLASH->PEKEYR = FLASH_PEKEY1;
        FLASH->PEKEYR = FLASH_PEKEY2;
    }

    if ((FLASH->PECR & FLASH_PECR_PELOCK) != 0U) {
        return BOOT_FLASH_ERROR;
    }

    if ((FLASH->PECR & FLASH_PECR_PRGLOCK) != 0U) {
        FLASH->PRGKEYR = FLASH_PRGKEY1;
        FLASH->PRGKEYR = FLASH_PRGKEY2;
    }

    if ((FLASH->PECR & FLASH_PECR_PRGLOCK) != 0U) {
        return BOOT_FLASH_ERROR;
    }

    return BOOT_FLASH_OK;
}

static void lock_flash(void)
{
    FLASH->PECR |= FLASH_PECR_PRGLOCK;
}

static uint32_t slot_base(uint32_t slot)
{
    return slot == BOOT_SLOT_A ? APP_SLOT_A_START_ADDR : APP_SLOT_B_START_ADDR;
}

static int slot_is_valid(uint32_t slot)
{
    return (slot == BOOT_SLOT_A) || (slot == BOOT_SLOT_B);
}

static boot_flash_result_t erase_page(uint32_t address)
{
    volatile uint32_t *page = (volatile uint32_t *)address;

    if (wait_ready() != BOOT_FLASH_OK) {
        return BOOT_FLASH_ERROR;
    }

    FLASH->PECR |= FLASH_PECR_ERASE | FLASH_PECR_PROG;
    *page = 0U;
    if (wait_ready() != BOOT_FLASH_OK) {
        FLASH->PECR &= ~(FLASH_PECR_ERASE | FLASH_PECR_PROG);
        return BOOT_FLASH_ERROR;
    }
    FLASH->PECR &= ~(FLASH_PECR_ERASE | FLASH_PECR_PROG);
    return BOOT_FLASH_OK;
}

boot_flash_result_t boot_flash_erase_slot(uint32_t slot, uint32_t image_size)
{
    uint32_t erase_size;

    if (!slot_is_valid(slot)) {
        return BOOT_FLASH_INVALID_ARGUMENT;
    }

    if ((image_size == 0U) || (image_size > APP_SLOT_SIZE)) {
        return BOOT_FLASH_INVALID_ARGUMENT;
    }

    if (unlock_flash() != BOOT_FLASH_OK) {
        return BOOT_FLASH_ERROR;
    }

    erase_size = (image_size + BOOT_FLASH_PAGE_SIZE - 1U) &
                 ~(BOOT_FLASH_PAGE_SIZE - 1U);
    for (uint32_t offset = 0U; offset < erase_size;
         offset += BOOT_FLASH_PAGE_SIZE) {
        if (erase_page(slot_base(slot) + offset) != BOOT_FLASH_OK) {
            lock_flash();
            return BOOT_FLASH_ERROR;
        }
    }

    lock_flash();
    return BOOT_FLASH_OK;
}

boot_flash_result_t boot_flash_write_slot(uint32_t slot,
                                          uint32_t offset,
                                          const uint8_t *data,
                                          size_t len)
{
    uint32_t address;

    if (!slot_is_valid(slot)) {
        return BOOT_FLASH_INVALID_ARGUMENT;
    }

    if (((len != 0U) && (data == 0)) || (offset > APP_SLOT_SIZE) ||
        (len > (APP_SLOT_SIZE - offset))) {
        return BOOT_FLASH_INVALID_ARGUMENT;
    }

    if ((offset & 0x3U) != 0U) {
        return BOOT_FLASH_INVALID_ARGUMENT;
    }

    if (unlock_flash() != BOOT_FLASH_OK) {
        return BOOT_FLASH_ERROR;
    }

    address = slot_base(slot) + offset;
    for (size_t index = 0U; index < len; index += sizeof(uint32_t)) {
        uint32_t word = UINT32_MAX;
        size_t remaining = len - index;
        size_t chunk = remaining < sizeof(uint32_t) ? remaining : sizeof(uint32_t);

        for (size_t byte = 0U; byte < chunk; byte++) {
            word &= ~((uint32_t)0xFFU << (byte * 8U));
            word |= (uint32_t)data[index + byte] << (byte * 8U);
        }

        if (wait_ready() != BOOT_FLASH_OK) {
            lock_flash();
            return BOOT_FLASH_ERROR;
        }
        *(volatile uint32_t *)(address + index) = word;
        if (wait_ready() != BOOT_FLASH_OK) {
            lock_flash();
            return BOOT_FLASH_ERROR;
        }
    }

    lock_flash();
    return BOOT_FLASH_OK;
}

const uint8_t *boot_flash_slot_base(uint32_t slot)
{
    if (!slot_is_valid(slot)) {
        return 0;
    }

    return (const uint8_t *)slot_base(slot);
}

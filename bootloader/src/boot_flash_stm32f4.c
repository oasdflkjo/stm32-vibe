#include "boot_flash.h"
#include "boot/boot_state.h"
#include "image/flash_layout.h"
#include "platform/device.h"

#define FLASH_KEY1 0x45670123U
#define FLASH_KEY2 0xCDEF89ABU
#define FLASH_ERROR_FLAGS (FLASH_SR_OPERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR | \
                           FLASH_SR_PGPERR | FLASH_SR_PGSERR)

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
    if ((FLASH->CR & FLASH_CR_LOCK) != 0U) {
        FLASH->KEYR = FLASH_KEY1;
        FLASH->KEYR = FLASH_KEY2;
    }
    return (FLASH->CR & FLASH_CR_LOCK) == 0U ? BOOT_FLASH_OK
                                             : BOOT_FLASH_ERROR;
}

static void lock_flash(void)
{
    FLASH->CR |= FLASH_CR_LOCK;
}

static int slot_is_valid(uint32_t slot)
{
    return (slot == BOOT_SLOT_A) || (slot == BOOT_SLOT_B);
}

static uint32_t slot_base(uint32_t slot)
{
    return slot == BOOT_SLOT_A ? APP_SLOT_A_START_ADDR : APP_SLOT_B_START_ADDR;
}

static uint32_t slot_sector(uint32_t slot)
{
    return slot == BOOT_SLOT_A ? 5U : 6U;
}

boot_flash_result_t boot_flash_erase_slot(uint32_t slot, uint32_t image_size)
{
    boot_flash_result_t result;
    if (!slot_is_valid(slot) || (image_size == 0U) ||
        (image_size > APP_SLOT_SIZE)) {
        return BOOT_FLASH_INVALID_ARGUMENT;
    }
    if (unlock_flash() != BOOT_FLASH_OK || wait_ready() != BOOT_FLASH_OK) {
        lock_flash();
        return BOOT_FLASH_ERROR;
    }
    FLASH->CR = (FLASH->CR & ~(FLASH_CR_SNB_Msk | FLASH_CR_PSIZE_Msk)) |
                FLASH_CR_SER | FLASH_CR_PSIZE_1 |
                (slot_sector(slot) << FLASH_CR_SNB_Pos);
    FLASH->CR |= FLASH_CR_STRT;
    result = wait_ready();
    FLASH->CR &= ~FLASH_CR_SER;
    lock_flash();
    return result;
}

boot_flash_result_t boot_flash_write_slot(uint32_t slot,
                                          uint32_t offset,
                                          const uint8_t *data,
                                          size_t len)
{
    uint32_t address;
    if (!slot_is_valid(slot) || ((len != 0U) && (data == 0)) ||
        (offset > APP_SLOT_SIZE) || (len > APP_SLOT_SIZE - offset) ||
        ((offset & 0x3U) != 0U)) {
        return BOOT_FLASH_INVALID_ARGUMENT;
    }
    if (unlock_flash() != BOOT_FLASH_OK) {
        return BOOT_FLASH_ERROR;
    }
    FLASH->CR = (FLASH->CR & ~FLASH_CR_PSIZE_Msk) |
                FLASH_CR_PG | FLASH_CR_PSIZE_1;
    address = slot_base(slot) + offset;
    for (size_t index = 0U; index < len; index += sizeof(uint32_t)) {
        uint32_t word = UINT32_MAX;
        size_t remaining = len - index;
        size_t chunk = remaining < sizeof(uint32_t) ? remaining
                                                    : sizeof(uint32_t);
        for (size_t byte = 0U; byte < chunk; byte++) {
            word &= ~((uint32_t)0xFFU << (byte * 8U));
            word |= (uint32_t)data[index + byte] << (byte * 8U);
        }
        *(volatile uint32_t *)(address + index) = word;
        if (wait_ready() != BOOT_FLASH_OK) {
            FLASH->CR &= ~FLASH_CR_PG;
            lock_flash();
            return BOOT_FLASH_ERROR;
        }
    }
    FLASH->CR &= ~FLASH_CR_PG;
    lock_flash();
    return BOOT_FLASH_OK;
}

const uint8_t *boot_flash_slot_base(uint32_t slot)
{
    return slot_is_valid(slot) ? (const uint8_t *)slot_base(slot) : 0;
}

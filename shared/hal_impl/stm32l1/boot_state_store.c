#include "boot/boot_state_store.h"
#include "image/flash_layout.h"
#include "stm32l1xx.h"

#define BOOT_STATE_COPY_A_ADDR BOOT_STATE_START_ADDR
#define BOOT_STATE_COPY_B_ADDR (BOOT_STATE_START_ADDR + 0x100U)
#define BOOT_FLASH_PAGE_SIZE 256U
#define FLASH_PEKEY1 0x89ABCDEFU
#define FLASH_PEKEY2 0x02030405U
#define FLASH_PRGKEY1 0x8C9DAEBFU
#define FLASH_PRGKEY2 0x13141516U
#define FLASH_ERROR_FLAGS \
    (FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_SIZERR | \
     FLASH_SR_OPTVERR | FLASH_SR_OPTVERRUSR)

static int wait_ready(void)
{
    while ((FLASH->SR & FLASH_SR_BSY) != 0U) {
    }

    if ((FLASH->SR & FLASH_ERROR_FLAGS) != 0U) {
        FLASH->SR = FLASH_ERROR_FLAGS;
        return 0;
    }

    if ((FLASH->SR & FLASH_SR_EOP) != 0U) {
        FLASH->SR = FLASH_SR_EOP;
    }

    return 1;
}

static int unlock_flash(void)
{
    if ((FLASH->PECR & FLASH_PECR_PELOCK) != 0U) {
        FLASH->PEKEYR = FLASH_PEKEY1;
        FLASH->PEKEYR = FLASH_PEKEY2;
    }

    if ((FLASH->PECR & FLASH_PECR_PELOCK) != 0U) {
        return 0;
    }

    if ((FLASH->PECR & FLASH_PECR_PRGLOCK) != 0U) {
        FLASH->PRGKEYR = FLASH_PRGKEY1;
        FLASH->PRGKEYR = FLASH_PRGKEY2;
    }

    return (FLASH->PECR & FLASH_PECR_PRGLOCK) == 0U;
}

static void lock_flash(void)
{
    FLASH->PECR |= FLASH_PECR_PRGLOCK;
    FLASH->PECR |= FLASH_PECR_PELOCK;
}

static int erase_page(uint32_t address)
{
    if (!wait_ready()) {
        return 0;
    }

    FLASH->PECR |= FLASH_PECR_ERASE | FLASH_PECR_PROG;
    *(volatile uint32_t *)(address & ~(BOOT_FLASH_PAGE_SIZE - 1U)) = 0U;
    if (!wait_ready()) {
        FLASH->PECR &= ~(FLASH_PECR_ERASE | FLASH_PECR_PROG);
        return 0;
    }
    FLASH->PECR &= ~(FLASH_PECR_ERASE | FLASH_PECR_PROG);
    return 1;
}

static int write_record(uint32_t address, const boot_state_record_t *state)
{
    const uint8_t *bytes = (const uint8_t *)state;

    if (!unlock_flash()) {
        return 0;
    }

    if (!erase_page(address)) {
        lock_flash();
        return 0;
    }

    for (uint32_t offset = 0U; offset < sizeof(*state);
         offset += sizeof(uint32_t)) {
        uint32_t word = (uint32_t)bytes[offset] |
                        ((uint32_t)bytes[offset + 1U] << 8U) |
                        ((uint32_t)bytes[offset + 2U] << 16U) |
                        ((uint32_t)bytes[offset + 3U] << 24U);

        if (!wait_ready()) {
            lock_flash();
            return 0;
        }
        *(volatile uint32_t *)(address + offset) = word;
        if (!wait_ready()) {
            lock_flash();
            return 0;
        }
    }

    lock_flash();
    return 1;
}

void boot_state_store_load(boot_state_record_t *state)
{
    const boot_state_record_t *first =
        (const boot_state_record_t *)BOOT_STATE_COPY_A_ADDR;
    const boot_state_record_t *second =
        (const boot_state_record_t *)BOOT_STATE_COPY_B_ADDR;

    if (!boot_state_select_latest(first, second, state)) {
        boot_state_init_default(state);
    }
}

int boot_state_store_save_next(const boot_state_record_t *state)
{
    uint32_t address = (state->generation & 1U) == 0U
                           ? BOOT_STATE_COPY_A_ADDR
                           : BOOT_STATE_COPY_B_ADDR;

    return write_record(address, state);
}

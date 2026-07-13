#include "boot/boot_state_store.h"
#include "image/flash_layout.h"
#include "platform/device.h"

#define BOOT_STATE_COPY_A_ADDR BOOT_STATE_START_ADDR
#define BOOT_STATE_COPY_B_ADDR (BOOT_STATE_START_ADDR + BOOT_STATE_COPY_STRIDE)
#define FLASH_KEY1 0x45670123U
#define FLASH_KEY2 0xCDEF89ABU
#define FLASH_ERROR_FLAGS (FLASH_SR_OPERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR | \
                           FLASH_SR_PGPERR | FLASH_SR_PGSERR)

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
    if ((FLASH->CR & FLASH_CR_LOCK) != 0U) {
        FLASH->KEYR = FLASH_KEY1;
        FLASH->KEYR = FLASH_KEY2;
    }
    return (FLASH->CR & FLASH_CR_LOCK) == 0U;
}

static void lock_flash(void)
{
    FLASH->CR |= FLASH_CR_LOCK;
}

static int erase_state_sector(uint32_t address)
{
    uint32_t sector = address == BOOT_STATE_COPY_A_ADDR ? 2U : 3U;
    if (!wait_ready()) {
        return 0;
    }
    FLASH->CR = (FLASH->CR & ~(FLASH_CR_SNB_Msk | FLASH_CR_PSIZE_Msk)) |
                FLASH_CR_SER | FLASH_CR_PSIZE_1 |
                (sector << FLASH_CR_SNB_Pos);
    FLASH->CR |= FLASH_CR_STRT;
    if (!wait_ready()) {
        FLASH->CR &= ~FLASH_CR_SER;
        return 0;
    }
    FLASH->CR &= ~FLASH_CR_SER;
    return 1;
}

static int write_record(uint32_t address, const boot_state_record_t *state)
{
    const uint32_t *words = (const uint32_t *)state;
    if (!unlock_flash() || !erase_state_sector(address)) {
        lock_flash();
        return 0;
    }
    FLASH->CR = (FLASH->CR & ~FLASH_CR_PSIZE_Msk) |
                FLASH_CR_PG | FLASH_CR_PSIZE_1;
    for (uint32_t index = 0U; index < sizeof(*state) / sizeof(uint32_t);
         index++) {
        *(volatile uint32_t *)(address + index * sizeof(uint32_t)) = words[index];
        if (!wait_ready()) {
            FLASH->CR &= ~FLASH_CR_PG;
            lock_flash();
            return 0;
        }
    }
    FLASH->CR &= ~FLASH_CR_PG;
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

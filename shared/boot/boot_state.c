#include "boot/boot_state.h"
#include <stddef.h>

static uint32_t crc32_update(uint32_t crc, uint8_t value)
{
    crc ^= value;

    for (uint32_t bit = 0U; bit < 8U; bit++) {
        crc = (crc & 1U) != 0U
                  ? (crc >> 1U) ^ 0xEDB88320U
                  : crc >> 1U;
    }

    return crc;
}

static int slot_is_valid(uint32_t slot)
{
    return (slot == BOOT_SLOT_A) || (slot == BOOT_SLOT_B);
}

static int optional_slot_is_valid(uint32_t slot)
{
    return (slot == BOOT_STATE_NO_SLOT) || slot_is_valid(slot);
}

static int slot_status_is_valid(uint32_t status)
{
    return (status == BOOT_SLOT_STATUS_EMPTY) ||
           (status == BOOT_SLOT_STATUS_VALID) ||
           (status == BOOT_SLOT_STATUS_PENDING) ||
           (status == BOOT_SLOT_STATUS_CONFIRMED) ||
           (status == BOOT_SLOT_STATUS_BAD);
}

void boot_state_init_default(boot_state_record_t *state)
{
    *state = (boot_state_record_t){
        .magic = BOOT_STATE_MAGIC,
        .format_version = BOOT_STATE_VERSION,
        .record_size = sizeof(*state),
        .generation = 0U,
        .active_slot = BOOT_SLOT_A,
        .pending_slot = BOOT_STATE_NO_SLOT,
        .pending_attempts = 0U,
        .slot_a_status = BOOT_SLOT_STATUS_EMPTY,
        .slot_b_status = BOOT_SLOT_STATUS_EMPTY,
    };
    boot_state_update_crc(state);
}

uint32_t boot_state_crc32(const boot_state_record_t *state)
{
    const uint8_t *bytes = (const uint8_t *)state;
    uint32_t crc = UINT32_MAX;

    for (uint32_t index = 0U; index < sizeof(*state); index++) {
        uint8_t value = bytes[index];

        if ((index >= offsetof(boot_state_record_t, record_crc32)) &&
            (index < offsetof(boot_state_record_t, record_crc32) +
                         sizeof(state->record_crc32))) {
            value = 0U;
        }

        crc = crc32_update(crc, value);
    }

    return crc ^ UINT32_MAX;
}

void boot_state_update_crc(boot_state_record_t *state)
{
    state->record_crc32 = 0U;
    state->record_crc32 = boot_state_crc32(state);
}

boot_state_status_t boot_state_validate(const boot_state_record_t *state)
{
    if (state->magic != BOOT_STATE_MAGIC) {
        return BOOT_STATE_BAD_MAGIC;
    }

    if (state->format_version != BOOT_STATE_VERSION) {
        return BOOT_STATE_BAD_VERSION;
    }

    if (state->record_size != sizeof(*state)) {
        return BOOT_STATE_BAD_SIZE;
    }

    if (!slot_is_valid(state->active_slot) ||
        !optional_slot_is_valid(state->pending_slot) ||
        !slot_status_is_valid(state->slot_a_status) ||
        !slot_status_is_valid(state->slot_b_status) ||
        (state->pending_attempts > BOOT_STATE_MAX_PENDING_ATTEMPTS)) {
        return BOOT_STATE_BAD_SLOT;
    }

    if (state->record_crc32 != boot_state_crc32(state)) {
        return BOOT_STATE_BAD_CRC;
    }

    return BOOT_STATE_VALID;
}

int boot_state_select_latest(const boot_state_record_t *first,
                             const boot_state_record_t *second,
                             boot_state_record_t *selected)
{
    int first_valid = boot_state_validate(first) == BOOT_STATE_VALID;
    int second_valid = boot_state_validate(second) == BOOT_STATE_VALID;

    if (!first_valid && !second_valid) {
        return 0;
    }

    if (first_valid &&
        (!second_valid || (first->generation >= second->generation))) {
        *selected = *first;
        return 1;
    }

    *selected = *second;
    return 1;
}

uint32_t boot_state_status_for_slot(const boot_state_record_t *state,
                                    uint32_t slot)
{
    if (slot == BOOT_SLOT_A) {
        return state->slot_a_status;
    }

    if (slot == BOOT_SLOT_B) {
        return state->slot_b_status;
    }

    return BOOT_SLOT_STATUS_BAD;
}

int boot_state_confirm_pending(boot_state_record_t *state)
{
    uint32_t pending_slot;

    if ((state == 0) || (state->pending_slot == BOOT_STATE_NO_SLOT)) {
        return 0;
    }

    pending_slot = state->pending_slot;
    if (pending_slot == BOOT_SLOT_A) {
        state->slot_a_status = BOOT_SLOT_STATUS_CONFIRMED;
        state->slot_b_status = BOOT_SLOT_STATUS_VALID;
    } else if (pending_slot == BOOT_SLOT_B) {
        state->slot_a_status = BOOT_SLOT_STATUS_VALID;
        state->slot_b_status = BOOT_SLOT_STATUS_CONFIRMED;
    } else {
        return 0;
    }

    state->active_slot = pending_slot;
    state->confirmed_version = state->reserved[BOOT_STATE_PENDING_VERSION_WORD];
    state->confirmed_image_crc32 =
        state->reserved[BOOT_STATE_PENDING_CRC32_WORD];
    state->reserved[BOOT_STATE_PENDING_VERSION_WORD] = 0U;
    state->reserved[BOOT_STATE_PENDING_CRC32_WORD] = 0U;
    state->pending_slot = BOOT_STATE_NO_SLOT;
    state->pending_attempts = 0U;
    state->generation++;
    boot_state_update_crc(state);
    return 1;
}

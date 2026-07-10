#include "boot_policy.h"
#include "image/flash_layout.h"

static uint32_t slot_base(uint32_t slot)
{
    return slot == BOOT_SLOT_B ? APP_SLOT_B_START_ADDR : APP_SLOT_A_START_ADDR;
}

static uint32_t slot_end(uint32_t slot)
{
    return slot == BOOT_SLOT_B ? APP_SLOT_B_END_ADDR : APP_SLOT_A_END_ADDR;
}

static uint32_t status_for_slot(const boot_state_record_t *state,
                                uint32_t slot)
{
    return boot_state_status_for_slot(state, slot);
}

static void set_status_for_slot(boot_state_record_t *state,
                                uint32_t slot,
                                uint32_t status)
{
    if (slot == BOOT_SLOT_A) {
        state->slot_a_status = status;
    } else if (slot == BOOT_SLOT_B) {
        state->slot_b_status = status;
    }
}

boot_candidate_t boot_policy_select_candidate(boot_state_record_t *state)
{
    uint32_t slot = state->active_slot;
    uint8_t boot_pending = 0U;

    if ((state->pending_slot != BOOT_STATE_NO_SLOT) &&
        (state->pending_attempts < BOOT_STATE_MAX_PENDING_ATTEMPTS) &&
        (status_for_slot(state, state->pending_slot) ==
         BOOT_SLOT_STATUS_PENDING)) {
        slot = state->pending_slot;
        boot_pending = 1U;
        state->pending_attempts++;
        state->generation++;
        boot_state_update_crc(state);
    }

    return (boot_candidate_t){
        .slot = slot,
        .image_base = slot_base(slot),
        .image_end = slot_end(slot),
        .boot_pending = boot_pending,
    };
}

void boot_policy_mark_pending_bad(boot_state_record_t *state)
{
    if (state->pending_slot != BOOT_STATE_NO_SLOT) {
        set_status_for_slot(state, state->pending_slot, BOOT_SLOT_STATUS_BAD);
        state->pending_slot = BOOT_STATE_NO_SLOT;
        state->pending_attempts = 0U;
        state->generation++;
        boot_state_update_crc(state);
    }
}

void boot_policy_mark_slot_b_pending(boot_state_record_t *state,
                                     uint32_t version,
                                     uint32_t image_crc32)
{
    state->pending_slot = BOOT_SLOT_B;
    state->pending_attempts = 0U;
    state->slot_b_status = BOOT_SLOT_STATUS_PENDING;
    if (state->slot_a_status == BOOT_SLOT_STATUS_CONFIRMED) {
        state->active_slot = BOOT_SLOT_A;
    }
    state->reserved[BOOT_STATE_PENDING_VERSION_WORD] = version;
    state->reserved[BOOT_STATE_PENDING_CRC32_WORD] = image_crc32;
    state->generation++;
    boot_state_update_crc(state);
}

void boot_policy_confirm_pending(boot_state_record_t *state)
{
    if (state->pending_slot == BOOT_STATE_NO_SLOT) {
        return;
    }

    set_status_for_slot(state, state->active_slot, BOOT_SLOT_STATUS_VALID);
    state->active_slot = state->pending_slot;
    set_status_for_slot(state, state->active_slot, BOOT_SLOT_STATUS_CONFIRMED);
    state->confirmed_version = state->reserved[BOOT_STATE_PENDING_VERSION_WORD];
    state->confirmed_image_crc32 =
        state->reserved[BOOT_STATE_PENDING_CRC32_WORD];
    state->reserved[BOOT_STATE_PENDING_VERSION_WORD] = 0U;
    state->reserved[BOOT_STATE_PENDING_CRC32_WORD] = 0U;
    state->pending_slot = BOOT_STATE_NO_SLOT;
    state->pending_attempts = 0U;
    state->generation++;
    boot_state_update_crc(state);
}

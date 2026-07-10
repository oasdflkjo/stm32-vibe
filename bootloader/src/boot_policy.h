#pragma once

#include "boot/boot_state.h"
#include <stdint.h>

typedef struct {
    uint32_t slot;
    uint32_t image_base;
    uint32_t image_end;
    uint8_t boot_pending;
} boot_candidate_t;

boot_candidate_t boot_policy_select_candidate(boot_state_record_t *state);
void boot_policy_mark_pending_bad(boot_state_record_t *state);
void boot_policy_mark_slot_b_pending(boot_state_record_t *state,
                                     uint32_t version,
                                     uint32_t image_crc32);
void boot_policy_confirm_pending(boot_state_record_t *state);

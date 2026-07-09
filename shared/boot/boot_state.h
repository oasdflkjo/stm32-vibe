#pragma once

#include <stdint.h>

#define BOOT_STATE_MAGIC 0x41545342U
#define BOOT_STATE_VERSION 1U
#define BOOT_STATE_RESERVED_WORDS 8U
#define BOOT_STATE_NO_SLOT UINT32_MAX
#define BOOT_STATE_MAX_PENDING_ATTEMPTS 3U

typedef enum {
    BOOT_SLOT_A = 0,
    BOOT_SLOT_B = 1,
} boot_slot_t;

typedef enum {
    BOOT_SLOT_STATUS_EMPTY = 0,
    BOOT_SLOT_STATUS_VALID = 1,
    BOOT_SLOT_STATUS_PENDING = 2,
    BOOT_SLOT_STATUS_CONFIRMED = 3,
    BOOT_SLOT_STATUS_BAD = 4,
} boot_slot_status_t;

typedef struct {
    uint32_t magic;
    uint32_t format_version;
    uint32_t record_size;
    uint32_t generation;
    uint32_t active_slot;
    uint32_t pending_slot;
    uint32_t pending_attempts;
    uint32_t slot_a_status;
    uint32_t slot_b_status;
    uint32_t confirmed_version;
    uint32_t confirmed_image_crc32;
    uint32_t record_crc32;
    uint32_t reserved[BOOT_STATE_RESERVED_WORDS];
} boot_state_record_t;

typedef enum {
    BOOT_STATE_VALID = 0,
    BOOT_STATE_BAD_MAGIC = 1,
    BOOT_STATE_BAD_VERSION = 2,
    BOOT_STATE_BAD_SIZE = 3,
    BOOT_STATE_BAD_SLOT = 4,
    BOOT_STATE_BAD_CRC = 5,
} boot_state_status_t;

void boot_state_init_default(boot_state_record_t *state);
uint32_t boot_state_crc32(const boot_state_record_t *state);
void boot_state_update_crc(boot_state_record_t *state);
boot_state_status_t boot_state_validate(const boot_state_record_t *state);
int boot_state_select_latest(const boot_state_record_t *first,
                             const boot_state_record_t *second,
                             boot_state_record_t *selected);
uint32_t boot_state_status_for_slot(const boot_state_record_t *state,
                                    uint32_t slot);

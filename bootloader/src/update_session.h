#pragma once

#include "update/update_protocol.h"
#include <stdint.h>

typedef struct {
    uint32_t session_id;
    uint32_t expected_image_size;
    uint32_t expected_image_crc32;
    uint32_t target_slot;
    uint32_t candidate_version;
    uint32_t candidate_crc32;
    uint32_t received_image_size;
    uint8_t session_active;
    uint8_t transfer_complete;
    uint8_t candidate_valid;
    uint8_t reset_requested;
} boot_update_session_t;

void boot_update_session_init(boot_update_session_t *session);
update_status_t boot_update_session_process(boot_update_session_t *session,
                                            const update_packet_t *packet);

#pragma once

#include "update/update_stream.h"
#include <stdint.h>

#define BOOT_UPDATE_UART_BAUD 115200U
#define BOOT_UPDATE_PROBE_POLLS 10000U

typedef struct {
    update_stream_t stream;
    uint8_t payload[UPDATE_PROTOCOL_MAX_PAYLOAD];
    uint32_t expected_image_size;
    uint32_t expected_image_crc32;
    uint32_t target_slot;
    uint32_t candidate_version;
    uint32_t candidate_crc32;
    uint32_t received_image_size;
    uint32_t packets_received;
    uint32_t parse_errors;
    uint32_t tx_errors;
    uint8_t session_active;
    uint8_t transfer_complete;
    uint8_t candidate_valid;
    uint8_t reset_requested;
} boot_update_loop_t;

typedef struct {
    uint32_t bytes_received;
    uint32_t packets_received;
    uint32_t parse_errors;
    uint32_t tx_errors;
} boot_update_poll_result_t;

void boot_update_loop_init(boot_update_loop_t *loop);
boot_update_poll_result_t boot_update_loop_poll(boot_update_loop_t *loop);
void boot_update_loop_run_forever(boot_update_loop_t *loop);

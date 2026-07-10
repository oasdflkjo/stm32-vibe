#pragma once

#include "update/update_stream.h"
#include "update_session.h"
#include <stdint.h>

#define BOOT_UPDATE_UART_BAUD 115200U
#define BOOT_UPDATE_PROBE_POLLS 10000U
#define BOOT_UPDATE_MAX_BYTES_PER_POLL (UPDATE_PROTOCOL_MAX_PACKET_SIZE * 2U)

typedef struct {
    update_stream_t stream;
    boot_update_session_t session;
    uint8_t payload[UPDATE_PROTOCOL_MAX_PAYLOAD];
    uint32_t packets_received;
    uint32_t parse_errors;
    uint32_t tx_errors;
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

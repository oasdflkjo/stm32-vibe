#pragma once

#include "hal/can.h"
#include "update/update_protocol.h"
#include <stddef.h>
#include <stdint.h>

#define UPDATE_CAN_REQUEST_BASE_ID 0x600U
#define UPDATE_CAN_RESPONSE_BASE_ID 0x680U
#define UPDATE_CAN_MAX_NODE_ID 0x7FU

#define UPDATE_CAN_FRAGMENT_START 0x80U
#define UPDATE_CAN_FRAGMENT_END 0x40U
#define UPDATE_CAN_FRAGMENT_INDEX_MASK 0x3FU

typedef enum {
    UPDATE_CAN_REASSEMBLY_NONE = 0,
    UPDATE_CAN_REASSEMBLY_COMPLETE = 1,
    UPDATE_CAN_REASSEMBLY_ERROR = 2,
} update_can_reassembly_result_t;

typedef struct {
    uint8_t packet[UPDATE_PROTOCOL_MAX_PACKET_SIZE];
    uint16_t expected_len;
    uint16_t received_len;
    uint8_t next_index;
    uint8_t active;
} update_can_reassembly_t;

typedef struct {
    const uint8_t *packet;
    size_t packet_len;
    size_t offset;
    uint32_t can_id;
    uint8_t fragment_index;
} update_can_fragmenter_t;

uint32_t update_can_request_id(uint8_t node_id);
uint32_t update_can_response_id(uint8_t node_id);

void update_can_reassembly_init(update_can_reassembly_t *reassembly);
update_can_reassembly_result_t update_can_reassembly_feed(
    update_can_reassembly_t *reassembly,
    const can_frame_t *frame,
    uint32_t expected_can_id,
    const uint8_t **packet,
    size_t *packet_len);

int update_can_fragmenter_init(update_can_fragmenter_t *fragmenter,
                               uint32_t can_id,
                               const uint8_t *packet,
                               size_t packet_len);
int update_can_fragmenter_next(update_can_fragmenter_t *fragmenter,
                               can_frame_t *frame);

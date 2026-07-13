#pragma once

#include "update/update_protocol.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t buffer[UPDATE_PROTOCOL_MAX_PACKET_SIZE];
    size_t len;
} update_stream_t;

void update_stream_init(update_stream_t *stream);
update_status_t update_stream_feed(update_stream_t *stream,
                                   const uint8_t *data,
                                   size_t data_len,
                                   update_packet_t *packet,
                                   uint8_t *payload,
                                   size_t payload_capacity,
                                   size_t *consumed);

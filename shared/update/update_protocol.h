#pragma once

#include <stddef.h>
#include <stdint.h>

#define UPDATE_PROTOCOL_SYNC 0x5544U
#define UPDATE_PROTOCOL_VERSION 1U
#define UPDATE_PROTOCOL_HEADER_SIZE 16U
#define UPDATE_PROTOCOL_CRC_SIZE 4U
#define UPDATE_PROTOCOL_MAX_PAYLOAD 128U
#define UPDATE_PROTOCOL_MAX_PACKET_SIZE \
    (UPDATE_PROTOCOL_HEADER_SIZE + UPDATE_PROTOCOL_MAX_PAYLOAD + \
     UPDATE_PROTOCOL_CRC_SIZE)

typedef enum {
    UPDATE_CMD_DISCOVER = 1,
    UPDATE_CMD_STATUS = 2,
    UPDATE_CMD_BEGIN = 3,
    UPDATE_CMD_BLOCK = 4,
    UPDATE_CMD_END = 5,
    UPDATE_CMD_VALIDATE = 6,
    UPDATE_CMD_ACTIVATE = 7,
    UPDATE_CMD_ABORT = 8,
    UPDATE_CMD_CONFIRM = 9,
    UPDATE_CMD_ACK = 10,
} update_command_t;

typedef enum {
    UPDATE_STATUS_OK = 0,
    UPDATE_STATUS_INVALID_ARGUMENT = 1,
    UPDATE_STATUS_BUFFER_TOO_SMALL = 2,
    UPDATE_STATUS_BAD_SYNC = 3,
    UPDATE_STATUS_BAD_VERSION = 4,
    UPDATE_STATUS_BAD_LENGTH = 5,
    UPDATE_STATUS_BAD_CRC = 6,
    UPDATE_STATUS_BAD_STATE = 7,
    UPDATE_STATUS_BAD_SEQUENCE = 8,
    UPDATE_STATUS_FLASH_ERROR = 9,
    UPDATE_STATUS_BAD_IMAGE = 10,
} update_status_t;

typedef struct {
    uint8_t command;
    uint8_t flags;
    uint16_t payload_len;
    uint32_t session_id;
    uint32_t sequence;
    const uint8_t *payload;
} update_packet_t;

update_status_t update_protocol_encode(const update_packet_t *packet,
                                        uint8_t *output,
                                        size_t output_capacity,
                                        size_t *output_len);
update_status_t update_protocol_decode(const uint8_t *input,
                                        size_t input_len,
                                        update_packet_t *packet,
                                        uint8_t *payload,
                                        size_t payload_capacity);
uint32_t update_protocol_crc32(const uint8_t *data, size_t len);

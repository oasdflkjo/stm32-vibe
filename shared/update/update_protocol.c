#include "update/update_protocol.h"

static uint16_t read_u16_le(const uint8_t *bytes)
{
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U);
}

static uint32_t read_u32_le(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) |
           ((uint32_t)bytes[3] << 24U);
}

static void write_u16_le(uint8_t *bytes, uint16_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
}

static void write_u32_le(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
    bytes[2] = (uint8_t)(value >> 16U);
    bytes[3] = (uint8_t)(value >> 24U);
}

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

uint32_t update_protocol_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = UINT32_MAX;

    for (size_t index = 0U; index < len; index++) {
        crc = crc32_update(crc, data[index]);
    }

    return crc ^ UINT32_MAX;
}

update_status_t update_protocol_encode(const update_packet_t *packet,
                                        uint8_t *output,
                                        size_t output_capacity,
                                        size_t *output_len)
{
    size_t total_len;
    uint32_t crc;

    if ((packet == 0) || (output == 0) || (output_len == 0) ||
        ((packet->payload_len != 0U) && (packet->payload == 0))) {
        return UPDATE_STATUS_INVALID_ARGUMENT;
    }

    if (packet->payload_len > UPDATE_PROTOCOL_MAX_PAYLOAD) {
        return UPDATE_STATUS_BAD_LENGTH;
    }

    total_len = UPDATE_PROTOCOL_HEADER_SIZE + packet->payload_len +
                UPDATE_PROTOCOL_CRC_SIZE;
    if (output_capacity < total_len) {
        return UPDATE_STATUS_BUFFER_TOO_SMALL;
    }

    write_u16_le(&output[0], UPDATE_PROTOCOL_SYNC);
    output[2] = UPDATE_PROTOCOL_VERSION;
    output[3] = packet->command;
    output[4] = packet->flags;
    output[5] = 0U;
    write_u16_le(&output[6], packet->payload_len);
    write_u32_le(&output[8], packet->session_id);
    write_u32_le(&output[12], packet->sequence);

    for (uint16_t index = 0U; index < packet->payload_len; index++) {
        output[UPDATE_PROTOCOL_HEADER_SIZE + index] = packet->payload[index];
    }

    crc = update_protocol_crc32(output,
                                UPDATE_PROTOCOL_HEADER_SIZE +
                                    packet->payload_len);
    write_u32_le(&output[UPDATE_PROTOCOL_HEADER_SIZE + packet->payload_len],
                 crc);

    *output_len = total_len;
    return UPDATE_STATUS_OK;
}

update_status_t update_protocol_decode(const uint8_t *input,
                                        size_t input_len,
                                        update_packet_t *packet,
                                        uint8_t *payload,
                                        size_t payload_capacity)
{
    uint16_t payload_len;
    size_t expected_len;
    uint32_t expected_crc;
    uint32_t actual_crc;

    if ((input == 0) || (packet == 0)) {
        return UPDATE_STATUS_INVALID_ARGUMENT;
    }

    if (input_len < UPDATE_PROTOCOL_HEADER_SIZE + UPDATE_PROTOCOL_CRC_SIZE) {
        return UPDATE_STATUS_BAD_LENGTH;
    }

    if (read_u16_le(&input[0]) != UPDATE_PROTOCOL_SYNC) {
        return UPDATE_STATUS_BAD_SYNC;
    }

    if (input[2] != UPDATE_PROTOCOL_VERSION) {
        return UPDATE_STATUS_BAD_VERSION;
    }

    payload_len = read_u16_le(&input[6]);
    if (payload_len > UPDATE_PROTOCOL_MAX_PAYLOAD) {
        return UPDATE_STATUS_BAD_LENGTH;
    }

    expected_len = UPDATE_PROTOCOL_HEADER_SIZE + payload_len +
                   UPDATE_PROTOCOL_CRC_SIZE;
    if (input_len != expected_len) {
        return UPDATE_STATUS_BAD_LENGTH;
    }

    if ((payload_len != 0U) && (payload == 0)) {
        return UPDATE_STATUS_INVALID_ARGUMENT;
    }

    if (payload_capacity < payload_len) {
        return UPDATE_STATUS_BUFFER_TOO_SMALL;
    }

    expected_crc =
        read_u32_le(&input[UPDATE_PROTOCOL_HEADER_SIZE + payload_len]);
    actual_crc = update_protocol_crc32(input,
                                       UPDATE_PROTOCOL_HEADER_SIZE +
                                           payload_len);
    if (actual_crc != expected_crc) {
        return UPDATE_STATUS_BAD_CRC;
    }

    for (uint16_t index = 0U; index < payload_len; index++) {
        payload[index] = input[UPDATE_PROTOCOL_HEADER_SIZE + index];
    }

    packet->command = input[3];
    packet->flags = input[4];
    packet->payload_len = payload_len;
    packet->session_id = read_u32_le(&input[8]);
    packet->sequence = read_u32_le(&input[12]);
    packet->payload = payload_len == 0U ? 0 : payload;
    return UPDATE_STATUS_OK;
}

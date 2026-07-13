#include "update/update_stream.h"

static int sync_at(const uint8_t *buffer, size_t len, size_t index)
{
    return (index + 1U < len) &&
           (buffer[index] == (uint8_t)UPDATE_PROTOCOL_SYNC) &&
           (buffer[index + 1U] == (uint8_t)(UPDATE_PROTOCOL_SYNC >> 8U));
}

static void drop_prefix(update_stream_t *stream, size_t count)
{
    if (count >= stream->len) {
        stream->len = 0U;
        return;
    }

    for (size_t index = 0U; index < stream->len - count; index++) {
        stream->buffer[index] = stream->buffer[index + count];
    }
    stream->len -= count;
}

static void resync(update_stream_t *stream)
{
    for (size_t index = 0U; index < stream->len; index++) {
        if (sync_at(stream->buffer, stream->len, index)) {
            drop_prefix(stream, index);
            return;
        }
    }

    if ((stream->len > 0U) &&
        (stream->buffer[stream->len - 1U] == (uint8_t)UPDATE_PROTOCOL_SYNC)) {
        stream->buffer[0] = stream->buffer[stream->len - 1U];
        stream->len = 1U;
        return;
    }

    stream->len = 0U;
}

static uint16_t read_payload_len(const uint8_t *buffer)
{
    return (uint16_t)buffer[6] | ((uint16_t)buffer[7] << 8U);
}

void update_stream_init(update_stream_t *stream)
{
    if (stream != 0) {
        stream->len = 0U;
    }
}

update_status_t update_stream_feed(update_stream_t *stream,
                                   const uint8_t *data,
                                   size_t data_len,
                                   update_packet_t *packet,
                                   uint8_t *payload,
                                   size_t payload_capacity,
                                   size_t *consumed)
{
    size_t input_index = 0U;

    if ((stream == 0) || ((data_len != 0U) && (data == 0)) ||
        (packet == 0) || (consumed == 0)) {
        return UPDATE_STATUS_INVALID_ARGUMENT;
    }

    *consumed = 0U;

    while (input_index < data_len) {
        if (stream->len == sizeof(stream->buffer)) {
            resync(stream);
            if (stream->len == sizeof(stream->buffer)) {
                stream->len = 0U;
            }
        }

        stream->buffer[stream->len] = data[input_index];
        stream->len++;
        input_index++;
        *consumed = input_index;

        resync(stream);

        if (stream->len < UPDATE_PROTOCOL_HEADER_SIZE) {
            continue;
        }

        uint16_t payload_len = read_payload_len(stream->buffer);
        if (payload_len > UPDATE_PROTOCOL_MAX_PAYLOAD) {
            drop_prefix(stream, 1U);
            resync(stream);
            continue;
        }

        size_t packet_len = UPDATE_PROTOCOL_HEADER_SIZE + payload_len +
                            UPDATE_PROTOCOL_CRC_SIZE;
        if (stream->len < packet_len) {
            continue;
        }

        update_status_t status =
            update_protocol_decode(stream->buffer, packet_len, packet,
                                   payload, payload_capacity);
        drop_prefix(stream, packet_len);
        if (status == UPDATE_STATUS_OK) {
            return UPDATE_STATUS_OK;
        }

        resync(stream);
        return status;
    }

    return UPDATE_STATUS_BAD_LENGTH;
}

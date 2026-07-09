#include "update/update_protocol.h"
#include "update/update_stream.h"
#include "unity.h"

static update_stream_t stream;
static uint8_t encoded[UPDATE_PROTOCOL_MAX_PACKET_SIZE];
static uint8_t encoded_second[UPDATE_PROTOCOL_MAX_PACKET_SIZE];
static uint8_t payload_buffer[UPDATE_PROTOCOL_MAX_PAYLOAD];

void setUp(void)
{
    update_stream_init(&stream);
}

void tearDown(void)
{
}

static size_t encode_packet(uint8_t command, uint32_t sequence)
{
    const uint8_t payload[] = {0xAAU, 0x55U};
    update_packet_t packet = {
        .command = command,
        .payload_len = sizeof(payload),
        .session_id = 0x12345678U,
        .sequence = sequence,
        .payload = payload,
    };
    size_t encoded_len = 0U;

    TEST_ASSERT_EQUAL(UPDATE_STATUS_OK,
                      update_protocol_encode(&packet, encoded,
                                             sizeof(encoded), &encoded_len));
    return encoded_len;
}

void test_stream_decodes_packet_split_across_feeds(void)
{
    size_t encoded_len = encode_packet(UPDATE_CMD_BLOCK, 4U);
    update_packet_t decoded = {0};
    size_t consumed = 0U;

    TEST_ASSERT_EQUAL(UPDATE_STATUS_BAD_LENGTH,
                      update_stream_feed(&stream, encoded, 5U, &decoded,
                                         payload_buffer,
                                         sizeof(payload_buffer), &consumed));
    TEST_ASSERT_EQUAL_UINT32(5U, consumed);

    TEST_ASSERT_EQUAL(UPDATE_STATUS_OK,
                      update_stream_feed(&stream, &encoded[5],
                                         encoded_len - 5U, &decoded,
                                         payload_buffer,
                                         sizeof(payload_buffer), &consumed));
    TEST_ASSERT_EQUAL_UINT8(UPDATE_CMD_BLOCK, decoded.command);
    TEST_ASSERT_EQUAL_UINT32(4U, decoded.sequence);
    TEST_ASSERT_EQUAL_UINT16(2U, decoded.payload_len);
    TEST_ASSERT_EQUAL_HEX8(0xAAU, decoded.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(0x55U, decoded.payload[1]);
}

void test_stream_ignores_noise_before_sync(void)
{
    const uint8_t noise[] = {0x00U, 0x11U, 0x22U, 0x33U};
    size_t encoded_len = encode_packet(UPDATE_CMD_STATUS, 2U);
    update_packet_t decoded = {0};
    size_t consumed = 0U;

    TEST_ASSERT_EQUAL(UPDATE_STATUS_BAD_LENGTH,
                      update_stream_feed(&stream, noise, sizeof(noise),
                                         &decoded, payload_buffer,
                                         sizeof(payload_buffer), &consumed));
    TEST_ASSERT_EQUAL(UPDATE_STATUS_OK,
                      update_stream_feed(&stream, encoded, encoded_len,
                                         &decoded, payload_buffer,
                                         sizeof(payload_buffer), &consumed));
    TEST_ASSERT_EQUAL_UINT8(UPDATE_CMD_STATUS, decoded.command);
}

void test_stream_preserves_partial_sync_after_noise(void)
{
    const uint8_t partial_sync[] = {0x00U, (uint8_t)UPDATE_PROTOCOL_SYNC};
    size_t encoded_len = encode_packet(UPDATE_CMD_DISCOVER, 1U);
    update_packet_t decoded = {0};
    size_t consumed = 0U;

    TEST_ASSERT_EQUAL(UPDATE_STATUS_BAD_LENGTH,
                      update_stream_feed(&stream, partial_sync,
                                         sizeof(partial_sync), &decoded,
                                         payload_buffer,
                                         sizeof(payload_buffer), &consumed));
    TEST_ASSERT_EQUAL(UPDATE_STATUS_OK,
                      update_stream_feed(&stream, &encoded[1],
                                         encoded_len - 1U, &decoded,
                                         payload_buffer,
                                         sizeof(payload_buffer), &consumed));
    TEST_ASSERT_EQUAL_UINT8(UPDATE_CMD_DISCOVER, decoded.command);
}

void test_stream_reports_bad_crc_and_recovers_on_next_feed(void)
{
    size_t encoded_len = encode_packet(UPDATE_CMD_BLOCK, 9U);
    update_packet_t decoded = {0};
    size_t consumed = 0U;

    encoded[encoded_len - 1U] ^= 0xFFU;
    TEST_ASSERT_EQUAL(UPDATE_STATUS_BAD_CRC,
                      update_stream_feed(&stream, encoded, encoded_len,
                                         &decoded, payload_buffer,
                                         sizeof(payload_buffer), &consumed));

    encoded_len = encode_packet(UPDATE_CMD_ABORT, 10U);
    TEST_ASSERT_EQUAL(UPDATE_STATUS_OK,
                      update_stream_feed(&stream, encoded, encoded_len,
                                         &decoded, payload_buffer,
                                         sizeof(payload_buffer), &consumed));
    TEST_ASSERT_EQUAL_UINT8(UPDATE_CMD_ABORT, decoded.command);
    TEST_ASSERT_EQUAL_UINT32(10U, decoded.sequence);
}

void test_stream_reports_consumed_bytes_for_back_to_back_packets(void)
{
    size_t first_len = encode_packet(UPDATE_CMD_STATUS, 1U);
    for (size_t index = 0U; index < first_len; index++) {
        encoded_second[index] = encoded[index];
    }
    size_t second_len = encode_packet(UPDATE_CMD_CONFIRM, 2U);
    update_packet_t decoded = {0};
    size_t consumed = 0U;

    uint8_t combined[UPDATE_PROTOCOL_MAX_PACKET_SIZE * 2U];
    for (size_t index = 0U; index < first_len; index++) {
        combined[index] = encoded_second[index];
    }
    for (size_t index = 0U; index < second_len; index++) {
        combined[first_len + index] = encoded[index];
    }

    TEST_ASSERT_EQUAL(UPDATE_STATUS_OK,
                      update_stream_feed(&stream, combined,
                                         first_len + second_len, &decoded,
                                         payload_buffer,
                                         sizeof(payload_buffer), &consumed));
    TEST_ASSERT_EQUAL_UINT8(UPDATE_CMD_STATUS, decoded.command);
    TEST_ASSERT_EQUAL_UINT32(first_len, consumed);

    TEST_ASSERT_EQUAL(UPDATE_STATUS_OK,
                      update_stream_feed(&stream, &combined[consumed],
                                         first_len + second_len - consumed,
                                         &decoded,
                                         payload_buffer,
                                         sizeof(payload_buffer), &consumed));
    TEST_ASSERT_EQUAL_UINT8(UPDATE_CMD_CONFIRM, decoded.command);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_stream_decodes_packet_split_across_feeds);
    RUN_TEST(test_stream_ignores_noise_before_sync);
    RUN_TEST(test_stream_preserves_partial_sync_after_noise);
    RUN_TEST(test_stream_reports_bad_crc_and_recovers_on_next_feed);
    RUN_TEST(test_stream_reports_consumed_bytes_for_back_to_back_packets);
    return UNITY_END();
}

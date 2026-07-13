#include "update/update_protocol.h"
#include "unity.h"

static uint8_t encoded[UPDATE_PROTOCOL_MAX_PACKET_SIZE];
static uint8_t decoded_payload[UPDATE_PROTOCOL_MAX_PAYLOAD];

void setUp(void)
{
}

void tearDown(void)
{
}

void test_encodes_and_decodes_packet(void)
{
    const uint8_t payload[] = {0x10U, 0x20U, 0x30U};
    update_packet_t packet = {
        .command = UPDATE_CMD_BLOCK,
        .flags = 0x80U,
        .payload_len = sizeof(payload),
        .session_id = 0x11223344U,
        .sequence = 7U,
        .payload = payload,
    };
    update_packet_t decoded = {0};
    size_t encoded_len = 0U;

    TEST_ASSERT_EQUAL(UPDATE_STATUS_OK,
                      update_protocol_encode(&packet, encoded,
                                             sizeof(encoded), &encoded_len));
    TEST_ASSERT_EQUAL_UINT32(UPDATE_PROTOCOL_HEADER_SIZE + sizeof(payload) +
                                 UPDATE_PROTOCOL_CRC_SIZE,
                             encoded_len);

    TEST_ASSERT_EQUAL(UPDATE_STATUS_OK,
                      update_protocol_decode(encoded, encoded_len, &decoded,
                                             decoded_payload,
                                             sizeof(decoded_payload)));
    TEST_ASSERT_EQUAL_UINT8(packet.command, decoded.command);
    TEST_ASSERT_EQUAL_UINT8(packet.flags, decoded.flags);
    TEST_ASSERT_EQUAL_UINT16(packet.payload_len, decoded.payload_len);
    TEST_ASSERT_EQUAL_UINT32(packet.session_id, decoded.session_id);
    TEST_ASSERT_EQUAL_UINT32(packet.sequence, decoded.sequence);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload, decoded.payload, sizeof(payload));
}

void test_encodes_zero_payload_packet(void)
{
    update_packet_t packet = {
        .command = UPDATE_CMD_DISCOVER,
        .session_id = 1U,
    };
    update_packet_t decoded = {0};
    size_t encoded_len = 0U;

    TEST_ASSERT_EQUAL(UPDATE_STATUS_OK,
                      update_protocol_encode(&packet, encoded,
                                             sizeof(encoded), &encoded_len));
    TEST_ASSERT_EQUAL(UPDATE_STATUS_OK,
                      update_protocol_decode(encoded, encoded_len, &decoded,
                                             decoded_payload,
                                             sizeof(decoded_payload)));
    TEST_ASSERT_EQUAL_UINT8(UPDATE_CMD_DISCOVER, decoded.command);
    TEST_ASSERT_EQUAL_UINT16(0U, decoded.payload_len);
    TEST_ASSERT_NULL(decoded.payload);
}

void test_rejects_bad_sync(void)
{
    update_packet_t packet = {
        .command = UPDATE_CMD_STATUS,
    };
    update_packet_t decoded = {0};
    size_t encoded_len = 0U;

    TEST_ASSERT_EQUAL(UPDATE_STATUS_OK,
                      update_protocol_encode(&packet, encoded,
                                             sizeof(encoded), &encoded_len));
    encoded[0] ^= 0xFFU;

    TEST_ASSERT_EQUAL(UPDATE_STATUS_BAD_SYNC,
                      update_protocol_decode(encoded, encoded_len, &decoded,
                                             decoded_payload,
                                             sizeof(decoded_payload)));
}

void test_rejects_bad_crc(void)
{
    update_packet_t packet = {
        .command = UPDATE_CMD_STATUS,
        .sequence = 1U,
    };
    update_packet_t decoded = {0};
    size_t encoded_len = 0U;

    TEST_ASSERT_EQUAL(UPDATE_STATUS_OK,
                      update_protocol_encode(&packet, encoded,
                                             sizeof(encoded), &encoded_len));
    encoded[12] ^= 0x01U;

    TEST_ASSERT_EQUAL(UPDATE_STATUS_BAD_CRC,
                      update_protocol_decode(encoded, encoded_len, &decoded,
                                             decoded_payload,
                                             sizeof(decoded_payload)));
}

void test_rejects_payload_larger_than_decoder_buffer(void)
{
    const uint8_t payload[] = {1U, 2U, 3U, 4U};
    update_packet_t packet = {
        .command = UPDATE_CMD_BLOCK,
        .payload_len = sizeof(payload),
        .payload = payload,
    };
    update_packet_t decoded = {0};
    size_t encoded_len = 0U;

    TEST_ASSERT_EQUAL(UPDATE_STATUS_OK,
                      update_protocol_encode(&packet, encoded,
                                             sizeof(encoded), &encoded_len));
    TEST_ASSERT_EQUAL(UPDATE_STATUS_BUFFER_TOO_SMALL,
                      update_protocol_decode(encoded, encoded_len, &decoded,
                                             decoded_payload, 2U));
}

void test_rejects_oversized_payload_on_encode(void)
{
    uint8_t payload[UPDATE_PROTOCOL_MAX_PAYLOAD + 1U] = {0};
    update_packet_t packet = {
        .command = UPDATE_CMD_BLOCK,
        .payload_len = sizeof(payload),
        .payload = payload,
    };
    size_t encoded_len = 0U;

    TEST_ASSERT_EQUAL(UPDATE_STATUS_BAD_LENGTH,
                      update_protocol_encode(&packet, encoded,
                                             sizeof(encoded), &encoded_len));
}

void test_rejects_short_frame(void)
{
    update_packet_t decoded = {0};

    TEST_ASSERT_EQUAL(UPDATE_STATUS_BAD_LENGTH,
                      update_protocol_decode(encoded, 3U, &decoded,
                                             decoded_payload,
                                             sizeof(decoded_payload)));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_encodes_and_decodes_packet);
    RUN_TEST(test_encodes_zero_payload_packet);
    RUN_TEST(test_rejects_bad_sync);
    RUN_TEST(test_rejects_bad_crc);
    RUN_TEST(test_rejects_payload_larger_than_decoder_buffer);
    RUN_TEST(test_rejects_oversized_payload_on_encode);
    RUN_TEST(test_rejects_short_frame);
    return UNITY_END();
}

#include "hal/uart_mock.h"
#include "update_agent.h"
#include "update/update_protocol.h"
#include "unity.h"

static uint32_t reset_count;

static void test_reset(void)
{
    reset_count++;
}

void setUp(void)
{
    reset_count = 0U;
    update_agent_init(test_reset);
}

void tearDown(void)
{
}

static void push_command(uint8_t command)
{
    uint8_t encoded[UPDATE_PROTOCOL_MAX_PACKET_SIZE];
    size_t encoded_len = 0U;
    update_packet_t packet = {
        .command = command,
        .session_id = 0x12345678U,
        .sequence = 2U,
    };

    TEST_ASSERT_EQUAL(UPDATE_STATUS_OK,
                      update_protocol_encode(&packet, encoded,
                                             sizeof(encoded), &encoded_len));
    TEST_ASSERT_EQUAL(UART_RESULT_OK, uart_mock_push_rx(encoded, encoded_len));
}

static update_packet_t pop_ack(uint8_t *payload)
{
    uint8_t encoded[UPDATE_PROTOCOL_MAX_PACKET_SIZE];
    size_t encoded_len;
    size_t packet_len;
    uint16_t payload_len;
    uint8_t byte;
    update_packet_t packet = {0};

    for (encoded_len = 0U; encoded_len < UPDATE_PROTOCOL_HEADER_SIZE;
         encoded_len++) {
        TEST_ASSERT_EQUAL(UART_RESULT_OK, uart_mock_pop_tx(&byte));
        encoded[encoded_len] = byte;
    }

    payload_len = (uint16_t)encoded[6] | ((uint16_t)encoded[7] << 8U);
    packet_len = UPDATE_PROTOCOL_HEADER_SIZE + payload_len +
                 UPDATE_PROTOCOL_CRC_SIZE;

    while (encoded_len < packet_len) {
        TEST_ASSERT_EQUAL(UART_RESULT_OK, uart_mock_pop_tx(&byte));
        encoded[encoded_len] = byte;
        encoded_len++;
    }

    TEST_ASSERT_EQUAL(UPDATE_STATUS_OK,
                      update_protocol_decode(encoded, encoded_len, &packet,
                                             payload,
                                             UPDATE_PROTOCOL_MAX_PAYLOAD));
    return packet;
}

void test_discover_acks_without_reset(void)
{
    uint8_t payload[UPDATE_PROTOCOL_MAX_PAYLOAD];

    push_command(UPDATE_CMD_DISCOVER);
    update_agent_poll();

    update_packet_t ack = pop_ack(payload);
    TEST_ASSERT_EQUAL_UINT8(UPDATE_CMD_ACK, ack.command);
    TEST_ASSERT_EQUAL_UINT8(UPDATE_CMD_DISCOVER, ack.payload[0]);
    TEST_ASSERT_EQUAL_UINT8(UPDATE_STATUS_OK, ack.payload[1]);
    TEST_ASSERT_EQUAL_UINT32(0U, reset_count);
}

void test_enter_update_acks_and_resets(void)
{
    uint8_t payload[UPDATE_PROTOCOL_MAX_PAYLOAD];

    push_command(UPDATE_CMD_ENTER_UPDATE);
    update_agent_poll();

    update_packet_t ack = pop_ack(payload);
    TEST_ASSERT_EQUAL_UINT8(UPDATE_CMD_ACK, ack.command);
    TEST_ASSERT_EQUAL_UINT8(UPDATE_CMD_ENTER_UPDATE, ack.payload[0]);
    TEST_ASSERT_EQUAL_UINT8(UPDATE_STATUS_OK, ack.payload[1]);
    TEST_ASSERT_EQUAL_UINT32(1U, reset_count);
}

void test_rejects_update_block_in_app(void)
{
    uint8_t payload[UPDATE_PROTOCOL_MAX_PAYLOAD];

    push_command(UPDATE_CMD_BLOCK);
    update_agent_poll();

    update_packet_t ack = pop_ack(payload);
    TEST_ASSERT_EQUAL_UINT8(UPDATE_CMD_BLOCK, ack.payload[0]);
    TEST_ASSERT_EQUAL_UINT8(UPDATE_STATUS_INVALID_ARGUMENT, ack.payload[1]);
    TEST_ASSERT_EQUAL_UINT32(0U, reset_count);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_discover_acks_without_reset);
    RUN_TEST(test_enter_update_acks_and_resets);
    RUN_TEST(test_rejects_update_block_in_app);
    return UNITY_END();
}

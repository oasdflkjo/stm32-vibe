#include "boot_flash_mock.h"
#include "boot_state_store_mock.h"
#include "hal/uart_mock.h"
#include "image/app_manifest.h"
#include "update_command.h"
#include "update/update_protocol.h"
#include "unity.h"
#include <stddef.h>
#include <string.h>

static boot_update_loop_t loop;
#define TEST_IMAGE_SIZE 0x260U
_Alignas(4) static uint8_t test_image[TEST_IMAGE_SIZE];

void setUp(void)
{
    TEST_ASSERT_EQUAL(UART_RESULT_OK, uart_init(BOOT_UPDATE_UART_BAUD));
    boot_flash_mock_reset();
    boot_state_store_mock_reset();
    boot_update_loop_init(&loop);
}

void tearDown(void)
{
}

static uint32_t image_crc32(void)
{
    const uint32_t crc_offset =
        APP_MANIFEST_OFFSET + offsetof(app_manifest_t, image_crc32);
    uint32_t crc = UINT32_MAX;

    for (uint32_t index = 0U; index < sizeof(test_image); index++) {
        uint8_t value = test_image[index];

        if ((index >= crc_offset) &&
            (index < crc_offset + sizeof(uint32_t))) {
            value = 0U;
        }

        crc ^= value;
        for (uint32_t bit = 0U; bit < 8U; bit++) {
            crc = (crc & 1U) != 0U
                      ? (crc >> 1U) ^ 0xEDB88320U
                      : crc >> 1U;
        }
    }

    return crc ^ UINT32_MAX;
}

static void write_u32_le(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8U);
    bytes[2] = (uint8_t)(value >> 16U);
    bytes[3] = (uint8_t)(value >> 24U);
}

static void set_confirmed_slot_a(void)
{
    boot_state_record_t state;

    boot_state_init_default(&state);
    state.slot_a_status = BOOT_SLOT_STATUS_CONFIRMED;
    boot_state_update_crc(&state);
    boot_state_store_mock_set_state(&state);
}

static uint32_t slot_start(uint32_t slot)
{
    return slot == BOOT_SLOT_A ? APP_SLOT_A_START_ADDR : APP_SLOT_B_START_ADDR;
}

static void prepare_valid_image(uint32_t slot)
{
    app_manifest_t *manifest;

    memset(test_image, 0xA5, sizeof(test_image));
    ((uint32_t *)test_image)[0] = 0x20014000U;
    ((uint32_t *)test_image)[1] = slot_start(slot) + 0x101U;
    manifest = (app_manifest_t *)(test_image + APP_MANIFEST_OFFSET);
    manifest->magic = APP_MANIFEST_MAGIC;
    manifest->manifest_version = APP_MANIFEST_VERSION;
    manifest->manifest_size = APP_MANIFEST_SIZE;
    manifest->image_size = sizeof(test_image);
    manifest->software_version = 9U;
    manifest->hardware_id = 0x152U;
    manifest->image_flags = 1U;
    manifest->reserved[APP_MANIFEST_APP_ID_WORD] = 0xAAAA5555U;
    manifest->reserved[APP_MANIFEST_BOARD_ID_WORD] = 0xBBBB6666U;
    manifest->image_crc32 = image_crc32();
}

static size_t encode_packet(uint8_t command,
                            uint32_t sequence,
                            const uint8_t *payload,
                            uint16_t payload_len,
                            uint8_t *output)
{
    update_packet_t packet = {
        .command = command,
        .session_id = 0xAABBCCDDU,
        .sequence = sequence,
        .payload_len = payload_len,
        .payload = payload,
    };
    size_t output_len = 0U;

    TEST_ASSERT_EQUAL(UPDATE_STATUS_OK,
                      update_protocol_encode(&packet, output,
                                             UPDATE_PROTOCOL_MAX_PACKET_SIZE,
                                             &output_len));
    return output_len;
}

static size_t encode_command(uint8_t command, uint8_t *output)
{
    return encode_packet(command, 7U, 0, 0U, output);
}

static void push_packet(uint8_t command,
                        uint32_t sequence,
                        const uint8_t *payload,
                        uint16_t payload_len)
{
    uint8_t encoded[UPDATE_PROTOCOL_MAX_PACKET_SIZE];
    size_t encoded_len =
        encode_packet(command, sequence, payload, payload_len, encoded);

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
    TEST_ASSERT_LESS_OR_EQUAL(sizeof(encoded), packet_len);

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

static uint32_t poll_until_packets(uint32_t expected_packets)
{
    uint32_t packets = 0U;

    for (uint32_t poll = 0U; poll < 16U; poll++) {
        boot_update_poll_result_t result = boot_update_loop_poll(&loop);

        packets += result.packets_received;
        if (packets >= expected_packets) {
            break;
        }
    }
    return packets;
}

void test_poll_feeds_split_packet_and_sends_ack(void)
{
    uint8_t encoded[UPDATE_PROTOCOL_MAX_PACKET_SIZE];
    uint8_t ack_payload[UPDATE_PROTOCOL_MAX_PAYLOAD];
    size_t encoded_len = encode_command(UPDATE_CMD_DISCOVER, encoded);

    TEST_ASSERT_EQUAL(UART_RESULT_OK, uart_mock_push_rx(encoded, 5U));
    boot_update_poll_result_t result = boot_update_loop_poll(&loop);
    TEST_ASSERT_EQUAL_UINT32(5U, result.bytes_received);
    TEST_ASSERT_EQUAL_UINT32(0U, result.packets_received);

    TEST_ASSERT_EQUAL(UART_RESULT_OK,
                      uart_mock_push_rx(&encoded[5], encoded_len - 5U));
    result = boot_update_loop_poll(&loop);
    TEST_ASSERT_EQUAL_UINT32(encoded_len - 5U, result.bytes_received);
    TEST_ASSERT_EQUAL_UINT32(1U, result.packets_received);

    update_packet_t ack = pop_ack(ack_payload);
    TEST_ASSERT_EQUAL_UINT8(UPDATE_CMD_ACK, ack.command);
    TEST_ASSERT_EQUAL_UINT32(0xAABBCCDDU, ack.session_id);
    TEST_ASSERT_EQUAL_UINT32(7U, ack.sequence);
    TEST_ASSERT_EQUAL_UINT8(UPDATE_CMD_DISCOVER, ack.payload[0]);
    TEST_ASSERT_EQUAL_UINT8(UPDATE_STATUS_OK, ack.payload[1]);
}

void test_poll_reports_bad_crc_and_sends_error_ack(void)
{
    uint8_t encoded[UPDATE_PROTOCOL_MAX_PACKET_SIZE];
    uint8_t ack_payload[UPDATE_PROTOCOL_MAX_PAYLOAD];
    size_t encoded_len = encode_command(UPDATE_CMD_STATUS, encoded);

    encoded[encoded_len - 1U] ^= 0xFFU;
    TEST_ASSERT_EQUAL(UART_RESULT_OK, uart_mock_push_rx(encoded, encoded_len));

    boot_update_poll_result_t result = boot_update_loop_poll(&loop);
    TEST_ASSERT_EQUAL_UINT32(encoded_len, result.bytes_received);
    TEST_ASSERT_EQUAL_UINT32(0U, result.packets_received);
    TEST_ASSERT_EQUAL_UINT32(1U, result.parse_errors);

    update_packet_t ack = pop_ack(ack_payload);
    TEST_ASSERT_EQUAL_UINT8(UPDATE_CMD_ACK, ack.command);
    TEST_ASSERT_EQUAL_UINT8(0U, ack.payload[0]);
    TEST_ASSERT_EQUAL_UINT8(UPDATE_STATUS_BAD_CRC, ack.payload[1]);
}

void test_poll_rejects_unknown_command(void)
{
    uint8_t encoded[UPDATE_PROTOCOL_MAX_PACKET_SIZE];
    uint8_t ack_payload[UPDATE_PROTOCOL_MAX_PAYLOAD];
    size_t encoded_len = encode_command(0xFEU, encoded);

    TEST_ASSERT_EQUAL(UART_RESULT_OK, uart_mock_push_rx(encoded, encoded_len));
    boot_update_poll_result_t result = boot_update_loop_poll(&loop);
    TEST_ASSERT_EQUAL_UINT32(1U, result.packets_received);

    update_packet_t ack = pop_ack(ack_payload);
    TEST_ASSERT_EQUAL_UINT8(UPDATE_CMD_ACK, ack.command);
    TEST_ASSERT_EQUAL_UINT8(0xFEU, ack.payload[0]);
    TEST_ASSERT_EQUAL_UINT8(UPDATE_STATUS_INVALID_ARGUMENT, ack.payload[1]);
}

void test_poll_rejects_remote_confirmation(void)
{
    uint8_t encoded[UPDATE_PROTOCOL_MAX_PACKET_SIZE];
    uint8_t ack_payload[UPDATE_PROTOCOL_MAX_PAYLOAD];
    size_t encoded_len = encode_command(UPDATE_CMD_CONFIRM, encoded);

    TEST_ASSERT_EQUAL(UART_RESULT_OK, uart_mock_push_rx(encoded, encoded_len));
    TEST_ASSERT_EQUAL_UINT32(1U, boot_update_loop_poll(&loop).packets_received);

    update_packet_t ack = pop_ack(ack_payload);
    TEST_ASSERT_EQUAL_UINT8(UPDATE_CMD_CONFIRM, ack.payload[0]);
    TEST_ASSERT_EQUAL_UINT8(UPDATE_STATUS_INVALID_ARGUMENT, ack.payload[1]);
    TEST_ASSERT_NULL(boot_state_store_mock_state());
}

void test_update_session_writes_and_validates_inactive_slot_b(void)
{
    uint8_t begin_payload[8];
    uint8_t ack_payload[UPDATE_PROTOCOL_MAX_PAYLOAD];

    set_confirmed_slot_a();
    prepare_valid_image(BOOT_SLOT_B);
    write_u32_le(&begin_payload[0], sizeof(test_image));
    write_u32_le(&begin_payload[4],
                 ((app_manifest_t *)(test_image + APP_MANIFEST_OFFSET))
                     ->image_crc32);

    push_packet(UPDATE_CMD_BEGIN, 0U, begin_payload, sizeof(begin_payload));
    push_packet(UPDATE_CMD_BLOCK, 0U, test_image, 128U);
    push_packet(UPDATE_CMD_BLOCK, 128U, &test_image[128], 128U);
    push_packet(UPDATE_CMD_BLOCK, 256U, &test_image[256], 128U);
    push_packet(UPDATE_CMD_BLOCK, 384U, &test_image[384], 128U);
    push_packet(UPDATE_CMD_BLOCK, 512U, &test_image[512],
                sizeof(test_image) - 512U);
    push_packet(UPDATE_CMD_END, 0U, 0, 0U);
    push_packet(UPDATE_CMD_VALIDATE, 0U, 0, 0U);

    TEST_ASSERT_EQUAL_UINT32(8U, poll_until_packets(8U));
    TEST_ASSERT_EQUAL_UINT32(sizeof(test_image), loop.session.received_image_size);
    TEST_ASSERT_EQUAL_UINT8(1U, loop.session.transfer_complete);
    TEST_ASSERT_EQUAL_UINT8(1U, loop.session.candidate_valid);
    TEST_ASSERT_EQUAL_UINT32(BOOT_SLOT_B, loop.session.target_slot);

    for (uint32_t index = 0U; index < 8U; index++) {
        update_packet_t ack = pop_ack(ack_payload);
        TEST_ASSERT_EQUAL_UINT8(UPDATE_CMD_ACK, ack.command);
        TEST_ASSERT_EQUAL_UINT8(UPDATE_STATUS_OK, ack.payload[1]);
    }
}

void test_block_before_begin_returns_bad_state(void)
{
    uint8_t data[] = {1U, 2U, 3U, 4U};
    uint8_t ack_payload[UPDATE_PROTOCOL_MAX_PAYLOAD];

    push_packet(UPDATE_CMD_BLOCK, 0U, data, sizeof(data));
    boot_update_poll_result_t result = boot_update_loop_poll(&loop);
    TEST_ASSERT_EQUAL_UINT32(1U, result.packets_received);

    update_packet_t ack = pop_ack(ack_payload);
    TEST_ASSERT_EQUAL_UINT8(UPDATE_CMD_BLOCK, ack.payload[0]);
    TEST_ASSERT_EQUAL_UINT8(UPDATE_STATUS_BAD_STATE, ack.payload[1]);
}

void test_out_of_order_block_returns_bad_sequence(void)
{
    uint8_t begin_payload[8];
    uint8_t data[] = {1U, 2U, 3U, 4U};
    uint8_t ack_payload[UPDATE_PROTOCOL_MAX_PAYLOAD];

    write_u32_le(&begin_payload[0], 16U);
    write_u32_le(&begin_payload[4], 0x12345678U);
    push_packet(UPDATE_CMD_BEGIN, 0U, begin_payload, sizeof(begin_payload));
    push_packet(UPDATE_CMD_BLOCK, 4U, data, sizeof(data));

    boot_update_poll_result_t result = boot_update_loop_poll(&loop);
    TEST_ASSERT_EQUAL_UINT32(2U, result.packets_received);

    update_packet_t begin_ack = pop_ack(ack_payload);
    TEST_ASSERT_EQUAL_UINT8(UPDATE_STATUS_OK, begin_ack.payload[1]);
    update_packet_t block_ack = pop_ack(ack_payload);
    TEST_ASSERT_EQUAL_UINT8(UPDATE_CMD_BLOCK, block_ack.payload[0]);
    TEST_ASSERT_EQUAL_UINT8(UPDATE_STATUS_BAD_SEQUENCE, block_ack.payload[1]);
}

void test_validate_rejects_corrupt_candidate(void)
{
    uint8_t begin_payload[8];
    uint8_t ack_payload[UPDATE_PROTOCOL_MAX_PAYLOAD];

    set_confirmed_slot_a();
    prepare_valid_image(BOOT_SLOT_B);
    write_u32_le(&begin_payload[0], sizeof(test_image));
    write_u32_le(&begin_payload[4], 0xDEADBEEFU);
    push_packet(UPDATE_CMD_BEGIN, 0U, begin_payload, sizeof(begin_payload));
    push_packet(UPDATE_CMD_BLOCK, 0U, test_image, 128U);
    push_packet(UPDATE_CMD_BLOCK, 128U, &test_image[128], 128U);
    push_packet(UPDATE_CMD_BLOCK, 256U, &test_image[256], 128U);
    push_packet(UPDATE_CMD_BLOCK, 384U, &test_image[384], 128U);
    push_packet(UPDATE_CMD_BLOCK, 512U, &test_image[512],
                sizeof(test_image) - 512U);
    push_packet(UPDATE_CMD_END, 0U, 0, 0U);
    push_packet(UPDATE_CMD_VALIDATE, 0U, 0, 0U);

    TEST_ASSERT_EQUAL_UINT32(8U, poll_until_packets(8U));
    for (uint32_t index = 0U; index < 7U; index++) {
        update_packet_t ack = pop_ack(ack_payload);
        TEST_ASSERT_EQUAL_UINT8(UPDATE_STATUS_OK, ack.payload[1]);
    }
    update_packet_t validate_ack = pop_ack(ack_payload);
    TEST_ASSERT_EQUAL_UINT8(UPDATE_CMD_VALIDATE, validate_ack.payload[0]);
    TEST_ASSERT_EQUAL_UINT8(UPDATE_STATUS_BAD_IMAGE, validate_ack.payload[1]);
    TEST_ASSERT_EQUAL_UINT8(0U, loop.session.candidate_valid);
}

void test_activate_marks_inactive_slot_b_pending_in_boot_state(void)
{
    uint8_t begin_payload[8];
    uint8_t ack_payload[UPDATE_PROTOCOL_MAX_PAYLOAD];

    set_confirmed_slot_a();
    prepare_valid_image(BOOT_SLOT_B);
    write_u32_le(&begin_payload[0], sizeof(test_image));
    write_u32_le(&begin_payload[4],
                 ((app_manifest_t *)(test_image + APP_MANIFEST_OFFSET))
                     ->image_crc32);
    push_packet(UPDATE_CMD_BEGIN, 0U, begin_payload, sizeof(begin_payload));
    push_packet(UPDATE_CMD_BLOCK, 0U, test_image, 128U);
    push_packet(UPDATE_CMD_BLOCK, 128U, &test_image[128], 128U);
    push_packet(UPDATE_CMD_BLOCK, 256U, &test_image[256], 128U);
    push_packet(UPDATE_CMD_BLOCK, 384U, &test_image[384], 128U);
    push_packet(UPDATE_CMD_BLOCK, 512U, &test_image[512],
                sizeof(test_image) - 512U);
    push_packet(UPDATE_CMD_END, 0U, 0, 0U);
    push_packet(UPDATE_CMD_VALIDATE, 0U, 0, 0U);
    push_packet(UPDATE_CMD_ACTIVATE, 0U, 0, 0U);

    TEST_ASSERT_EQUAL_UINT32(9U, poll_until_packets(9U));
    for (uint32_t index = 0U; index < 9U; index++) {
        update_packet_t ack = pop_ack(ack_payload);
        TEST_ASSERT_EQUAL_UINT8(UPDATE_STATUS_OK, ack.payload[1]);
    }

    const boot_state_record_t *saved = boot_state_store_mock_state();
    TEST_ASSERT_NOT_NULL(saved);
    TEST_ASSERT_EQUAL_UINT32(BOOT_SLOT_B, saved->pending_slot);
    TEST_ASSERT_EQUAL_UINT32(BOOT_SLOT_STATUS_PENDING, saved->slot_b_status);
    TEST_ASSERT_EQUAL_UINT32(0U, saved->confirmed_version);
    TEST_ASSERT_EQUAL_UINT32(9U,
                             saved->reserved[BOOT_STATE_PENDING_VERSION_WORD]);
}

void test_first_update_targets_slot_a_when_no_app_is_confirmed(void)
{
    uint8_t begin_payload[8];
    uint8_t ack_payload[UPDATE_PROTOCOL_MAX_PAYLOAD];

    prepare_valid_image(BOOT_SLOT_A);
    write_u32_le(&begin_payload[0], sizeof(test_image));
    write_u32_le(&begin_payload[4],
                 ((app_manifest_t *)(test_image + APP_MANIFEST_OFFSET))
                     ->image_crc32);
    push_packet(UPDATE_CMD_BEGIN, 0U, begin_payload, sizeof(begin_payload));
    push_packet(UPDATE_CMD_BLOCK, 0U, test_image, 128U);
    push_packet(UPDATE_CMD_BLOCK, 128U, &test_image[128], 128U);
    push_packet(UPDATE_CMD_BLOCK, 256U, &test_image[256], 128U);
    push_packet(UPDATE_CMD_BLOCK, 384U, &test_image[384], 128U);
    push_packet(UPDATE_CMD_BLOCK, 512U, &test_image[512],
                sizeof(test_image) - 512U);
    push_packet(UPDATE_CMD_END, 0U, 0, 0U);
    push_packet(UPDATE_CMD_VALIDATE, 0U, 0, 0U);
    push_packet(UPDATE_CMD_ACTIVATE, 0U, 0, 0U);

    TEST_ASSERT_EQUAL_UINT32(9U, poll_until_packets(9U));
    for (uint32_t index = 0U; index < 9U; index++) {
        update_packet_t ack = pop_ack(ack_payload);
        TEST_ASSERT_EQUAL_UINT8(UPDATE_STATUS_OK, ack.payload[1]);
    }

    const boot_state_record_t *saved = boot_state_store_mock_state();
    TEST_ASSERT_NOT_NULL(saved);
    TEST_ASSERT_EQUAL_UINT32(BOOT_SLOT_A, loop.session.target_slot);
    TEST_ASSERT_EQUAL_UINT32(BOOT_SLOT_A, saved->pending_slot);
    TEST_ASSERT_EQUAL_UINT32(BOOT_SLOT_STATUS_PENDING, saved->slot_a_status);
    TEST_ASSERT_EQUAL_UINT32(BOOT_SLOT_STATUS_EMPTY, saved->slot_b_status);
}

void test_update_session_targets_slot_a_when_slot_b_is_active(void)
{
    uint8_t begin_payload[8];
    uint8_t ack_payload[UPDATE_PROTOCOL_MAX_PAYLOAD];
    boot_state_record_t state;

    boot_state_init_default(&state);
    state.active_slot = BOOT_SLOT_B;
    state.slot_a_status = BOOT_SLOT_STATUS_VALID;
    state.slot_b_status = BOOT_SLOT_STATUS_CONFIRMED;
    boot_state_update_crc(&state);
    boot_state_store_mock_set_state(&state);

    prepare_valid_image(BOOT_SLOT_A);
    write_u32_le(&begin_payload[0], sizeof(test_image));
    write_u32_le(&begin_payload[4],
                 ((app_manifest_t *)(test_image + APP_MANIFEST_OFFSET))
                     ->image_crc32);
    push_packet(UPDATE_CMD_BEGIN, 0U, begin_payload, sizeof(begin_payload));
    push_packet(UPDATE_CMD_BLOCK, 0U, test_image, 128U);
    push_packet(UPDATE_CMD_BLOCK, 128U, &test_image[128], 128U);
    push_packet(UPDATE_CMD_BLOCK, 256U, &test_image[256], 128U);
    push_packet(UPDATE_CMD_BLOCK, 384U, &test_image[384], 128U);
    push_packet(UPDATE_CMD_BLOCK, 512U, &test_image[512],
                sizeof(test_image) - 512U);
    push_packet(UPDATE_CMD_END, 0U, 0, 0U);
    push_packet(UPDATE_CMD_VALIDATE, 0U, 0, 0U);
    push_packet(UPDATE_CMD_ACTIVATE, 0U, 0, 0U);

    TEST_ASSERT_EQUAL_UINT32(9U, poll_until_packets(9U));
    for (uint32_t index = 0U; index < 9U; index++) {
        update_packet_t ack = pop_ack(ack_payload);
        TEST_ASSERT_EQUAL_UINT8(UPDATE_STATUS_OK, ack.payload[1]);
    }

    const boot_state_record_t *saved = boot_state_store_mock_state();
    TEST_ASSERT_NOT_NULL(saved);
    TEST_ASSERT_EQUAL_UINT32(BOOT_SLOT_A, loop.session.target_slot);
    TEST_ASSERT_EQUAL_UINT32(BOOT_SLOT_A, saved->pending_slot);
    TEST_ASSERT_EQUAL_UINT32(BOOT_SLOT_STATUS_PENDING, saved->slot_a_status);
    TEST_ASSERT_EQUAL_UINT32(BOOT_SLOT_STATUS_CONFIRMED, saved->slot_b_status);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_poll_feeds_split_packet_and_sends_ack);
    RUN_TEST(test_poll_reports_bad_crc_and_sends_error_ack);
    RUN_TEST(test_poll_rejects_unknown_command);
    RUN_TEST(test_poll_rejects_remote_confirmation);
    RUN_TEST(test_update_session_writes_and_validates_inactive_slot_b);
    RUN_TEST(test_block_before_begin_returns_bad_state);
    RUN_TEST(test_out_of_order_block_returns_bad_sequence);
    RUN_TEST(test_validate_rejects_corrupt_candidate);
    RUN_TEST(test_activate_marks_inactive_slot_b_pending_in_boot_state);
    RUN_TEST(test_first_update_targets_slot_a_when_no_app_is_confirmed);
    RUN_TEST(test_update_session_targets_slot_a_when_slot_b_is_active);
    return UNITY_END();
}

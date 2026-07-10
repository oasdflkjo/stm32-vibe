#include "update_command.h"
#include "app_image.h"
#include "app_validation.h"
#include "boot_flash.h"
#include "boot_policy.h"
#include "boot_state_store.h"
#include "hal/uart.h"
#include "image/flash_layout.h"

static uint32_t read_u32_le(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) |
           ((uint32_t)bytes[3] << 24U);
}

static int command_is_supported(uint8_t command)
{
    return (command == UPDATE_CMD_DISCOVER) ||
           (command == UPDATE_CMD_STATUS) ||
           (command == UPDATE_CMD_BEGIN) ||
           (command == UPDATE_CMD_BLOCK) ||
           (command == UPDATE_CMD_END) ||
           (command == UPDATE_CMD_VALIDATE) ||
           (command == UPDATE_CMD_ACTIVATE) ||
           (command == UPDATE_CMD_ABORT) ||
           (command == UPDATE_CMD_CONFIRM) ||
           (command == UPDATE_CMD_ENTER_UPDATE);
}

static void send_ack(boot_update_loop_t *loop,
                     uint8_t command,
                     update_status_t status,
                     uint32_t session_id,
                     uint32_t sequence)
{
    uint8_t payload[] = {command, (uint8_t)status};
    uint8_t encoded[UPDATE_PROTOCOL_MAX_PACKET_SIZE];
    size_t encoded_len = 0U;
    update_packet_t ack = {
        .command = UPDATE_CMD_ACK,
        .payload_len = sizeof(payload),
        .session_id = session_id,
        .sequence = sequence,
        .payload = payload,
    };

    if (update_protocol_encode(&ack, encoded, sizeof(encoded), &encoded_len) !=
        UPDATE_STATUS_OK) {
        loop->tx_errors++;
        return;
    }

    if (uart_send(encoded, encoded_len) != UART_RESULT_OK) {
        loop->tx_errors++;
    }
}

static update_status_t handle_begin(boot_update_loop_t *loop,
                                    const update_packet_t *packet)
{
    uint32_t image_size;
    uint32_t image_crc32;

    if (packet->payload_len != 8U) {
        return UPDATE_STATUS_BAD_LENGTH;
    }

    image_size = read_u32_le(&packet->payload[0]);
    image_crc32 = read_u32_le(&packet->payload[4]);
    if ((image_size == 0U) || (image_size > APP_SLOT_SIZE)) {
        return UPDATE_STATUS_BAD_LENGTH;
    }

    if (boot_flash_erase_slot_b(image_size) != BOOT_FLASH_OK) {
        return UPDATE_STATUS_FLASH_ERROR;
    }

    loop->expected_image_size = image_size;
    loop->expected_image_crc32 = image_crc32;
    loop->received_image_size = 0U;
    loop->candidate_version = 0U;
    loop->candidate_crc32 = 0U;
    loop->session_active = 1U;
    loop->transfer_complete = 0U;
    loop->candidate_valid = 0U;
    return UPDATE_STATUS_OK;
}

static update_status_t handle_block(boot_update_loop_t *loop,
                                    const update_packet_t *packet)
{
    if (loop->session_active == 0U) {
        return UPDATE_STATUS_BAD_STATE;
    }

    if ((packet->payload_len == 0U) ||
        (packet->sequence != loop->received_image_size) ||
        (packet->payload_len >
         (loop->expected_image_size - loop->received_image_size))) {
        return UPDATE_STATUS_BAD_SEQUENCE;
    }

    if (boot_flash_write_slot_b(packet->sequence, packet->payload,
                                packet->payload_len) != BOOT_FLASH_OK) {
        return UPDATE_STATUS_FLASH_ERROR;
    }

    loop->received_image_size += packet->payload_len;
    return UPDATE_STATUS_OK;
}

static update_status_t handle_end(boot_update_loop_t *loop)
{
    if (loop->session_active == 0U) {
        return UPDATE_STATUS_BAD_STATE;
    }

    if (loop->received_image_size != loop->expected_image_size) {
        return UPDATE_STATUS_BAD_LENGTH;
    }

    loop->transfer_complete = 1U;
    return UPDATE_STATUS_OK;
}

static update_status_t handle_validate(boot_update_loop_t *loop)
{
    app_image_result_t image_result;
    const uint32_t *vectors = (const uint32_t *)boot_flash_slot_b_base();

    if ((loop->session_active == 0U) || (loop->transfer_complete == 0U)) {
        return UPDATE_STATUS_BAD_STATE;
    }

    image_result = app_image_validate(boot_flash_slot_b_base(), APP_SLOT_SIZE);
    if ((image_result.status != APP_IMAGE_VALID) ||
        (image_result.image_size != loop->expected_image_size) ||
        (image_result.expected_crc32 != loop->expected_image_crc32)) {
        return UPDATE_STATUS_BAD_IMAGE;
    }

    if (!app_vectors_are_valid_for_slot(vectors[0], vectors[1],
                                        APP_SLOT_B_START_ADDR,
                                        APP_SLOT_B_END_ADDR)) {
        return UPDATE_STATUS_BAD_IMAGE;
    }

    loop->candidate_version = image_result.version;
    loop->candidate_crc32 = image_result.expected_crc32;
    loop->candidate_valid = 1U;
    return UPDATE_STATUS_OK;
}

static update_status_t handle_activate(boot_update_loop_t *loop)
{
    boot_state_record_t state;

    if (loop->candidate_valid == 0U) {
        return UPDATE_STATUS_BAD_STATE;
    }

    boot_state_store_load(&state);
    boot_policy_mark_slot_b_pending(&state, loop->candidate_version,
                                    loop->candidate_crc32);
    if (!boot_state_store_save_next(&state)) {
        return UPDATE_STATUS_FLASH_ERROR;
    }

    loop->reset_requested = 1U;
    return UPDATE_STATUS_OK;
}

static update_status_t handle_confirm(void)
{
    boot_state_record_t state;

    boot_state_store_load(&state);
    boot_policy_confirm_pending(&state);
    if (!boot_state_store_save_next(&state)) {
        return UPDATE_STATUS_FLASH_ERROR;
    }

    return UPDATE_STATUS_OK;
}

static update_status_t handle_packet(boot_update_loop_t *loop,
                                     const update_packet_t *packet)
{
    switch (packet->command) {
    case UPDATE_CMD_DISCOVER:
    case UPDATE_CMD_STATUS:
    case UPDATE_CMD_ENTER_UPDATE:
        return UPDATE_STATUS_OK;
    case UPDATE_CMD_BEGIN:
        return handle_begin(loop, packet);
    case UPDATE_CMD_BLOCK:
        return handle_block(loop, packet);
    case UPDATE_CMD_END:
        return handle_end(loop);
    case UPDATE_CMD_VALIDATE:
        return handle_validate(loop);
    case UPDATE_CMD_ABORT:
        loop->session_active = 0U;
        loop->transfer_complete = 0U;
        loop->candidate_valid = 0U;
        loop->expected_image_size = 0U;
        loop->expected_image_crc32 = 0U;
        loop->candidate_version = 0U;
        loop->candidate_crc32 = 0U;
        loop->received_image_size = 0U;
        return UPDATE_STATUS_OK;
    case UPDATE_CMD_ACTIVATE:
        return handle_activate(loop);
    case UPDATE_CMD_CONFIRM:
        return handle_confirm();
    default:
        return UPDATE_STATUS_INVALID_ARGUMENT;
    }
}

void boot_update_loop_init(boot_update_loop_t *loop)
{
    if (loop != 0) {
        *loop = (boot_update_loop_t){0};
        update_stream_init(&loop->stream);
    }
}

boot_update_poll_result_t boot_update_loop_poll(boot_update_loop_t *loop)
{
    boot_update_poll_result_t result = {0};
    uint8_t byte;
    uart_result_t rx_result;
    uint32_t initial_tx_errors;

    if (loop == 0) {
        return result;
    }

    initial_tx_errors = loop->tx_errors;

    while ((rx_result = uart_receive_byte(&byte)) == UART_RESULT_OK) {
        update_packet_t packet = {0};
        size_t consumed = 0U;
        update_status_t status;

        result.bytes_received++;
        status = update_stream_feed(&loop->stream, &byte, 1U, &packet,
                                    loop->payload, sizeof(loop->payload),
                                    &consumed);
        (void)consumed;

        if (status == UPDATE_STATUS_OK) {
            update_status_t ack_status = command_is_supported(packet.command)
                                             ? handle_packet(loop, &packet)
                                             : UPDATE_STATUS_INVALID_ARGUMENT;
            loop->packets_received++;
            result.packets_received++;
            send_ack(loop, packet.command, ack_status, packet.session_id,
                     packet.sequence);
        } else if (status != UPDATE_STATUS_BAD_LENGTH) {
            loop->parse_errors++;
            result.parse_errors++;
            send_ack(loop, 0U, status, 0U, 0U);
        }
    }

    result.tx_errors = loop->tx_errors - initial_tx_errors;
    return result;
}

void boot_update_loop_run_forever(boot_update_loop_t *loop)
{
    while (1) {
        (void)boot_update_loop_poll(loop);
    }
}

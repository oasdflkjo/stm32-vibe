#include "update_command.h"

#include "hal/uart.h"

static update_status_t encode_ack(uint8_t command,
                                  update_status_t status,
                                  uint32_t session_id,
                                  uint32_t sequence,
                                  uint8_t *encoded,
                                  size_t encoded_capacity,
                                  size_t *encoded_len)
{
    uint8_t payload[] = {command, (uint8_t)status};
    update_packet_t ack = {
        .command = UPDATE_CMD_ACK,
        .payload_len = sizeof(payload),
        .session_id = session_id,
        .sequence = sequence,
        .payload = payload,
    };

    return update_protocol_encode(&ack, encoded, encoded_capacity, encoded_len);
}

static void send_uart_ack(boot_update_loop_t *loop,
                          uint8_t command,
                          update_status_t status,
                          uint32_t session_id,
                          uint32_t sequence)
{
    uint8_t encoded[UPDATE_PROTOCOL_MAX_PACKET_SIZE];
    size_t encoded_len = 0U;

    if ((encode_ack(command, status, session_id, sequence, encoded,
                    sizeof(encoded), &encoded_len) != UPDATE_STATUS_OK) ||
        (uart_send(encoded, encoded_len) != UART_RESULT_OK)) {
        loop->tx_errors++;
    }
}

update_status_t boot_update_loop_process_encoded(
    boot_update_loop_t *loop,
    const uint8_t *encoded,
    size_t encoded_len,
    uint8_t *ack,
    size_t ack_capacity,
    size_t *ack_len)
{
    update_packet_t packet = {0};
    update_status_t decode_status;
    update_status_t command_status;

    if ((loop == 0) || (encoded == 0) || (ack == 0) || (ack_len == 0)) {
        return UPDATE_STATUS_INVALID_ARGUMENT;
    }
    decode_status = update_protocol_decode(encoded, encoded_len, &packet,
                                            loop->payload,
                                            sizeof(loop->payload));
    if (decode_status != UPDATE_STATUS_OK) {
        loop->parse_errors++;
        return encode_ack(0U, decode_status, 0U, 0U, ack, ack_capacity,
                          ack_len);
    }
    command_status = boot_update_session_process(&loop->session, &packet);
    loop->packets_received++;
    return encode_ack(packet.command, command_status, packet.session_id,
                      packet.sequence, ack, ack_capacity, ack_len);
}

void boot_update_loop_init(boot_update_loop_t *loop)
{
    if (loop != 0) {
        *loop = (boot_update_loop_t){0};
        update_stream_init(&loop->stream);
        boot_update_session_init(&loop->session);
    }
}

boot_update_poll_result_t boot_update_loop_poll(boot_update_loop_t *loop)
{
    boot_update_poll_result_t result = {0};
    uint8_t byte;
    uint32_t initial_tx_errors;

    if (loop == 0) {
        return result;
    }
    initial_tx_errors = loop->tx_errors;

    for (uint32_t byte_count = 0U;
         byte_count < BOOT_UPDATE_MAX_BYTES_PER_POLL;
         byte_count++) {
        update_packet_t packet = {0};
        size_t consumed = 0U;
        update_status_t status;

        if (uart_receive_byte(&byte) != UART_RESULT_OK) {
            break;
        }
        result.bytes_received++;
        status = update_stream_feed(&loop->stream, &byte, 1U, &packet,
                                    loop->payload, sizeof(loop->payload),
                                    &consumed);
        (void)consumed;
        if (status == UPDATE_STATUS_OK) {
            update_status_t ack_status =
                boot_update_session_process(&loop->session, &packet);
            loop->packets_received++;
            result.packets_received++;
            send_uart_ack(loop, packet.command, ack_status, packet.session_id,
                          packet.sequence);
        } else if (status != UPDATE_STATUS_BAD_LENGTH) {
            loop->parse_errors++;
            result.parse_errors++;
            send_uart_ack(loop, 0U, status, 0U, 0U);
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

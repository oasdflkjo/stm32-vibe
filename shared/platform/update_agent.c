#include "platform/update_agent.h"
#include "hal/uart.h"
#include "update/update_stream.h"

static update_stream_t stream;
static uint8_t payload[UPDATE_PROTOCOL_MAX_PAYLOAD];
static update_agent_reset_fn_t reset_callback;

static void send_ack(uint8_t command,
                     update_status_t status,
                     uint32_t session_id,
                     uint32_t sequence)
{
    uint8_t ack_payload[] = {command, (uint8_t)status};
    uint8_t encoded[UPDATE_PROTOCOL_MAX_PACKET_SIZE];
    size_t encoded_len = 0U;
    update_packet_t ack = {
        .command = UPDATE_CMD_ACK,
        .payload_len = sizeof(ack_payload),
        .session_id = session_id,
        .sequence = sequence,
        .payload = ack_payload,
    };

    if (update_protocol_encode(&ack, encoded, sizeof(encoded), &encoded_len) ==
        UPDATE_STATUS_OK) {
        (void)uart_send(encoded, encoded_len);
    }
}

void update_agent_init(update_agent_reset_fn_t reset_fn)
{
    update_stream_init(&stream);
    reset_callback = reset_fn;
    (void)uart_init(UPDATE_AGENT_UART_BAUD);
}

void update_agent_poll(void)
{
    uint8_t byte;

    while (uart_receive_byte(&byte) == UART_RESULT_OK) {
        update_packet_t packet = {0};
        size_t consumed = 0U;
        update_status_t status =
            update_stream_feed(&stream, &byte, 1U, &packet, payload,
                               sizeof(payload), &consumed);
        (void)consumed;

        if (status == UPDATE_STATUS_OK) {
            if (packet.command == UPDATE_CMD_DISCOVER) {
                send_ack(packet.command, UPDATE_STATUS_OK, packet.session_id,
                         packet.sequence);
            } else if (packet.command == UPDATE_CMD_ENTER_UPDATE) {
                send_ack(packet.command, UPDATE_STATUS_OK, packet.session_id,
                         packet.sequence);
                (void)uart_drain_tx();
                if (reset_callback != 0) {
                    reset_callback();
                }
            } else {
                send_ack(packet.command, UPDATE_STATUS_INVALID_ARGUMENT,
                         packet.session_id, packet.sequence);
            }
        } else if (status != UPDATE_STATUS_BAD_LENGTH) {
            send_ack(0U, status, 0U, 0U);
        }
    }
}

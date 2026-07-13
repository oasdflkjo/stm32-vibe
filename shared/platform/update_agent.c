#include "platform/update_agent.h"
#include "hal/uart.h"
#include "update/update_stream.h"
#if defined(BOARD_HAS_CAN) && !defined(ENABLE_CAN_SMOKE_TEST)
#include "hal/can.h"
#include "update/update_can.h"
#endif

static update_stream_t stream;
static uint8_t payload[UPDATE_PROTOCOL_MAX_PAYLOAD];
static update_agent_reset_fn_t reset_callback;
#if defined(BOARD_HAS_CAN) && !defined(ENABLE_CAN_SMOKE_TEST)
static update_can_reassembly_t can_reassembly;
#endif

static size_t encode_ack(uint8_t command,
                         update_status_t status,
                         uint32_t session_id,
                         uint32_t sequence,
                         uint8_t *encoded,
                         size_t capacity)
{
    uint8_t ack_payload[] = {command, (uint8_t)status};
    size_t encoded_len = 0U;
    update_packet_t ack = {
        .command = UPDATE_CMD_ACK,
        .payload_len = sizeof(ack_payload),
        .session_id = session_id,
        .sequence = sequence,
        .payload = ack_payload,
    };

    return update_protocol_encode(&ack, encoded, capacity, &encoded_len) ==
                   UPDATE_STATUS_OK
               ? encoded_len
               : 0U;
}

static void send_uart_ack(uint8_t command,
                          update_status_t status,
                          uint32_t session_id,
                          uint32_t sequence)
{
    uint8_t encoded[UPDATE_PROTOCOL_MAX_PACKET_SIZE];
    size_t encoded_len = encode_ack(command, status, session_id, sequence,
                                    encoded, sizeof(encoded));
    if (encoded_len != 0U) {
        (void)uart_send(encoded, encoded_len);
    }
}

#if defined(BOARD_HAS_CAN) && !defined(ENABLE_CAN_SMOKE_TEST)
static void send_can_ack(uint8_t command,
                         update_status_t status,
                         uint32_t session_id,
                         uint32_t sequence)
{
    uint8_t encoded[UPDATE_PROTOCOL_MAX_PACKET_SIZE];
    size_t encoded_len = encode_ack(command, status, session_id, sequence,
                                    encoded, sizeof(encoded));
    update_can_fragmenter_t fragmenter;
    can_frame_t frame;

    if ((encoded_len == 0U) ||
        !update_can_fragmenter_init(&fragmenter,
                                    update_can_response_id(UPDATE_CAN_NODE_ID),
                                    encoded, encoded_len)) {
        return;
    }
    while (update_can_fragmenter_next(&fragmenter, &frame)) {
        uint32_t retry = 100000U;
        can_result_t result;
        do {
            result = can_send(&frame);
        } while ((result == CAN_RESULT_TX_FULL) && (retry-- != 0U));
        if (result != CAN_RESULT_OK) {
            return;
        }
    }
}
#endif

void update_agent_init(update_agent_reset_fn_t reset_fn)
{
    update_stream_init(&stream);
    reset_callback = reset_fn;
    (void)uart_init(UPDATE_AGENT_UART_BAUD);
#if defined(BOARD_HAS_CAN) && !defined(ENABLE_CAN_SMOKE_TEST)
    update_can_reassembly_init(&can_reassembly);
    (void)can_init(500000U);
#endif
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
                send_uart_ack(packet.command, UPDATE_STATUS_OK,
                              packet.session_id, packet.sequence);
            } else if (packet.command == UPDATE_CMD_ENTER_UPDATE) {
                send_uart_ack(packet.command, UPDATE_STATUS_OK,
                              packet.session_id, packet.sequence);
                (void)uart_drain_tx();
                if (reset_callback != 0) {
                    reset_callback();
                }
            } else {
                send_uart_ack(packet.command,
                              UPDATE_STATUS_INVALID_ARGUMENT,
                              packet.session_id, packet.sequence);
            }
        } else if (status != UPDATE_STATUS_BAD_LENGTH) {
            send_uart_ack(0U, status, 0U, 0U);
        }
    }

#if defined(BOARD_HAS_CAN) && !defined(ENABLE_CAN_SMOKE_TEST)
    {
        can_frame_t frame;
        while (can_receive(&frame) == CAN_RESULT_OK) {
            const uint8_t *encoded = 0;
            size_t encoded_len = 0U;
            update_can_reassembly_result_t result =
                update_can_reassembly_feed(
                    &can_reassembly, &frame,
                    update_can_request_id(UPDATE_CAN_NODE_ID), &encoded,
                    &encoded_len);
            if (result == UPDATE_CAN_REASSEMBLY_COMPLETE) {
                update_packet_t packet = {0};
                update_status_t decode_status = update_protocol_decode(
                    encoded, encoded_len, &packet, payload, sizeof(payload));
                if (decode_status != UPDATE_STATUS_OK) {
                    send_can_ack(0U, decode_status, 0U, 0U);
                } else if ((packet.command == UPDATE_CMD_DISCOVER) ||
                           (packet.command == UPDATE_CMD_ENTER_UPDATE)) {
                    send_can_ack(packet.command, UPDATE_STATUS_OK,
                                 packet.session_id, packet.sequence);
                    if (packet.command == UPDATE_CMD_ENTER_UPDATE) {
                        (void)can_drain_tx();
                        if (reset_callback != 0) {
                            reset_callback();
                        }
                    }
                } else {
                    send_can_ack(packet.command,
                                 UPDATE_STATUS_INVALID_ARGUMENT,
                                 packet.session_id, packet.sequence);
                }
            }
        }
    }
#endif
}

#include "update_can_loop.h"

#include "hal/can.h"

#define CAN_TX_RETRY_LIMIT 100000U
#define CAN_RX_FRAMES_PER_POLL 32U

static int send_frame_wait(const can_frame_t *frame)
{
    for (uint32_t retry = 0U; retry < CAN_TX_RETRY_LIMIT; retry++) {
        can_result_t result = can_send(frame);
        if (result == CAN_RESULT_OK) {
            return 1;
        }
        if (result != CAN_RESULT_TX_FULL) {
            return 0;
        }
    }
    return 0;
}

static void send_ack(uint8_t node_id, const uint8_t *ack, size_t ack_len)
{
    update_can_fragmenter_t fragmenter;
    can_frame_t frame;

    if (!update_can_fragmenter_init(&fragmenter,
                                    update_can_response_id(node_id),
                                    ack, ack_len)) {
        return;
    }
    while (update_can_fragmenter_next(&fragmenter, &frame)) {
        if (!send_frame_wait(&frame)) {
            return;
        }
    }
}

int boot_can_update_loop_init(boot_can_update_loop_t *can_loop,
                              uint8_t node_id,
                              uint32_t bitrate)
{
    if ((can_loop == 0) || (node_id > UPDATE_CAN_MAX_NODE_ID) ||
        (can_init(bitrate) != CAN_RESULT_OK)) {
        return 0;
    }
    *can_loop = (boot_can_update_loop_t){.node_id = node_id,
                                         .initialized = 1U};
    update_can_reassembly_init(&can_loop->reassembly);
    return 1;
}

uint32_t boot_can_update_loop_poll(boot_can_update_loop_t *can_loop,
                                   boot_update_loop_t *update_loop)
{
    uint32_t frames_received = 0U;
    uint32_t frames_polled = 0U;
    can_frame_t frame;

    if ((can_loop == 0) || (update_loop == 0) ||
        (can_loop->initialized == 0U)) {
        return 0U;
    }
    while ((frames_polled < CAN_RX_FRAMES_PER_POLL) &&
           (can_receive(&frame) == CAN_RESULT_OK)) {
        const uint8_t *encoded = 0;
        size_t encoded_len = 0U;
        update_can_reassembly_result_t result;

        frames_polled++;
        if (frame.id != update_can_request_id(can_loop->node_id)) {
            continue;
        }
        frames_received++;
        result = update_can_reassembly_feed(
            &can_loop->reassembly, &frame,
            update_can_request_id(can_loop->node_id), &encoded, &encoded_len);
        if (result == UPDATE_CAN_REASSEMBLY_COMPLETE) {
            uint8_t ack[UPDATE_PROTOCOL_MAX_PACKET_SIZE];
            size_t ack_len = 0U;
            if (boot_update_loop_process_encoded(
                    update_loop, encoded, encoded_len, ack, sizeof(ack),
                    &ack_len) == UPDATE_STATUS_OK) {
                send_ack(can_loop->node_id, ack, ack_len);
            }
        }
    }
    return frames_received;
}

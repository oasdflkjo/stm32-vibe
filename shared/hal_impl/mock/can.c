#include "hal/can_mock.h"
#include <string.h>

#define CAN_MOCK_QUEUE_LEN 8U

typedef struct {
    can_frame_t frames[CAN_MOCK_QUEUE_LEN];
    uint8_t head;
    uint8_t count;
} can_mock_queue_t;

static can_mock_queue_t tx_queue;
static can_mock_queue_t rx_queue;
static can_status_t status;

static can_result_t queue_push(can_mock_queue_t *queue,
                               const can_frame_t *frame)
{
    uint8_t index;

    if ((frame == NULL) || (frame->dlc > CAN_MAX_DATA_LEN)) {
        return CAN_RESULT_INVALID_ARGUMENT;
    }

    if (queue->count == CAN_MOCK_QUEUE_LEN) {
        return CAN_RESULT_TX_FULL;
    }

    index = (uint8_t)((queue->head + queue->count) % CAN_MOCK_QUEUE_LEN);
    queue->frames[index] = *frame;
    queue->count++;
    return CAN_RESULT_OK;
}

static can_result_t queue_pop(can_mock_queue_t *queue, can_frame_t *frame)
{
    if (frame == NULL) {
        return CAN_RESULT_INVALID_ARGUMENT;
    }

    if (queue->count == 0U) {
        return CAN_RESULT_RX_EMPTY;
    }

    *frame = queue->frames[queue->head];
    queue->head = (uint8_t)((queue->head + 1U) % CAN_MOCK_QUEUE_LEN);
    queue->count--;
    return CAN_RESULT_OK;
}

void can_mock_reset(void)
{
    memset(&tx_queue, 0, sizeof(tx_queue));
    memset(&rx_queue, 0, sizeof(rx_queue));
    memset(&status, 0, sizeof(status));
}

can_result_t can_init(uint32_t bitrate)
{
    if (bitrate == 0U) {
        return CAN_RESULT_INVALID_ARGUMENT;
    }

    can_mock_reset();
    status.initialized = 1U;
    return CAN_RESULT_OK;
}

can_result_t can_send(const can_frame_t *frame)
{
    can_result_t result;

    if (status.bus_off != 0U) {
        status.error_count++;
        return CAN_RESULT_BUS_OFF;
    }

    if (status.initialized == 0U) {
        return CAN_RESULT_NOT_READY;
    }

    result = queue_push(&tx_queue, frame);
    if (result == CAN_RESULT_OK) {
        status.tx_count++;
    }
    return result;
}

can_result_t can_receive(can_frame_t *frame)
{
    can_result_t result;

    if (status.bus_off != 0U) {
        status.error_count++;
        return CAN_RESULT_BUS_OFF;
    }

    if (status.initialized == 0U) {
        return CAN_RESULT_NOT_READY;
    }

    result = queue_pop(&rx_queue, frame);
    if (result == CAN_RESULT_OK) {
        status.rx_count++;
    }
    return result;
}

can_result_t can_drain_tx(void)
{
    if (status.bus_off != 0U) {
        return CAN_RESULT_BUS_OFF;
    }
    return status.initialized != 0U ? CAN_RESULT_OK : CAN_RESULT_NOT_READY;
}

can_status_t can_get_status(void)
{
    return status;
}

can_result_t can_mock_push_rx(const can_frame_t *frame)
{
    return queue_push(&rx_queue, frame);
}

can_result_t can_mock_pop_tx(can_frame_t *frame)
{
    return queue_pop(&tx_queue, frame);
}

void can_mock_set_bus_off(int bus_off)
{
    status.bus_off = bus_off ? 1U : 0U;
}

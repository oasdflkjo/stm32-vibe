#include "hal/uart_mock.h"
#include <string.h>

#define UART_MOCK_QUEUE_LEN 4096U

typedef struct {
    uint8_t bytes[UART_MOCK_QUEUE_LEN];
    size_t head;
    size_t count;
} uart_mock_queue_t;

static uart_mock_queue_t rx_queue;
static uart_mock_queue_t tx_queue;
static uint8_t initialized;

static uart_result_t queue_push(uart_mock_queue_t *queue, uint8_t byte)
{
    size_t index;

    if (queue->count == UART_MOCK_QUEUE_LEN) {
        return UART_RESULT_TX_FULL;
    }

    index = (queue->head + queue->count) % UART_MOCK_QUEUE_LEN;
    queue->bytes[index] = byte;
    queue->count++;
    return UART_RESULT_OK;
}

static uart_result_t queue_pop(uart_mock_queue_t *queue, uint8_t *byte)
{
    if (byte == 0) {
        return UART_RESULT_INVALID_ARGUMENT;
    }

    if (queue->count == 0U) {
        return UART_RESULT_RX_EMPTY;
    }

    *byte = queue->bytes[queue->head];
    queue->head = (queue->head + 1U) % UART_MOCK_QUEUE_LEN;
    queue->count--;
    return UART_RESULT_OK;
}

void uart_mock_reset(void)
{
    memset(&rx_queue, 0, sizeof(rx_queue));
    memset(&tx_queue, 0, sizeof(tx_queue));
    initialized = 0U;
}

uart_result_t uart_init(uint32_t baudrate)
{
    if (baudrate == 0U) {
        return UART_RESULT_INVALID_ARGUMENT;
    }

    uart_mock_reset();
    initialized = 1U;
    return UART_RESULT_OK;
}

uart_result_t uart_receive_byte(uint8_t *byte)
{
    if (initialized == 0U) {
        return UART_RESULT_NOT_READY;
    }

    return queue_pop(&rx_queue, byte);
}

uart_result_t uart_send(const uint8_t *data, size_t len)
{
    if ((len != 0U) && (data == 0)) {
        return UART_RESULT_INVALID_ARGUMENT;
    }

    if (initialized == 0U) {
        return UART_RESULT_NOT_READY;
    }

    for (size_t index = 0U; index < len; index++) {
        uart_result_t result = queue_push(&tx_queue, data[index]);
        if (result != UART_RESULT_OK) {
            return result;
        }
    }

    return UART_RESULT_OK;
}

uart_result_t uart_mock_push_rx(const uint8_t *data, size_t len)
{
    if ((len != 0U) && (data == 0)) {
        return UART_RESULT_INVALID_ARGUMENT;
    }

    for (size_t index = 0U; index < len; index++) {
        uart_result_t result = queue_push(&rx_queue, data[index]);
        if (result != UART_RESULT_OK) {
            return result;
        }
    }

    return UART_RESULT_OK;
}

uart_result_t uart_mock_pop_tx(uint8_t *byte)
{
    return queue_pop(&tx_queue, byte);
}

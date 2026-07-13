#pragma once

#include <stddef.h>
#include <stdint.h>

typedef enum {
    UART_RESULT_OK = 0,
    UART_RESULT_INVALID_ARGUMENT = 1,
    UART_RESULT_NOT_READY = 2,
    UART_RESULT_RX_EMPTY = 3,
    UART_RESULT_TX_FULL = 4,
} uart_result_t;

uart_result_t uart_init(uint32_t baudrate);
uart_result_t uart_receive_byte(uint8_t *byte);
uart_result_t uart_send(const uint8_t *data, size_t len);
uart_result_t uart_drain_tx(void);

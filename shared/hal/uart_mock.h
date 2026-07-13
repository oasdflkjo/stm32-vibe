#pragma once

#include "hal/uart.h"

void uart_mock_reset(void);
uart_result_t uart_mock_push_rx(const uint8_t *data, size_t len);
uart_result_t uart_mock_pop_tx(uint8_t *byte);

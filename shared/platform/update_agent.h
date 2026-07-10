#pragma once

#include <stdint.h>

#define UPDATE_AGENT_UART_BAUD 115200U

typedef void (*update_agent_reset_fn_t)(void);

void update_agent_init(update_agent_reset_fn_t reset_fn);
void update_agent_poll(void);

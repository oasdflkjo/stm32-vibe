#pragma once

#include "hal/can.h"

void can_mock_reset(void);
can_result_t can_mock_push_rx(const can_frame_t *frame);
can_result_t can_mock_pop_tx(can_frame_t *frame);
void can_mock_set_bus_off(int bus_off);

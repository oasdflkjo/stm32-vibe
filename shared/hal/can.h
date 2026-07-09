#pragma once

#include <stdint.h>

#define CAN_MAX_DATA_LEN 8U

typedef enum {
    CAN_RESULT_OK = 0,
    CAN_RESULT_INVALID_ARGUMENT = 1,
    CAN_RESULT_NOT_READY = 2,
    CAN_RESULT_TX_FULL = 3,
    CAN_RESULT_RX_EMPTY = 4,
    CAN_RESULT_BUS_OFF = 5,
} can_result_t;

typedef struct {
    uint32_t id;
    uint8_t dlc;
    uint8_t data[CAN_MAX_DATA_LEN];
} can_frame_t;

typedef struct {
    uint32_t tx_count;
    uint32_t rx_count;
    uint32_t error_count;
    uint8_t initialized;
    uint8_t bus_off;
} can_status_t;

can_result_t can_init(uint32_t bitrate);
can_result_t can_send(const can_frame_t *frame);
can_result_t can_receive(can_frame_t *frame);
can_status_t can_get_status(void);

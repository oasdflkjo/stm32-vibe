#pragma once

#include "update/update_can.h"
#include "update_command.h"
#include <stdint.h>

typedef struct {
    update_can_reassembly_t reassembly;
    uint8_t node_id;
    uint8_t initialized;
} boot_can_update_loop_t;

int boot_can_update_loop_init(boot_can_update_loop_t *can_loop,
                              uint8_t node_id,
                              uint32_t bitrate);
uint32_t boot_can_update_loop_poll(boot_can_update_loop_t *can_loop,
                                   boot_update_loop_t *update_loop);

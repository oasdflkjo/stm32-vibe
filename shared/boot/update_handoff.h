#pragma once

#include <stdint.h>

#define UPDATE_HANDOFF_MAGIC 0x55504454U
#define UPDATE_HANDOFF_ADDR  0x20013F00U

static inline volatile uint32_t *update_handoff_word(void)
{
    return (volatile uint32_t *)UPDATE_HANDOFF_ADDR;
}

static inline void update_handoff_request(void)
{
    *update_handoff_word() = UPDATE_HANDOFF_MAGIC;
}

static inline int update_handoff_take(void)
{
    if (*update_handoff_word() != UPDATE_HANDOFF_MAGIC) {
        return 0;
    }

    *update_handoff_word() = 0U;
    return 1;
}

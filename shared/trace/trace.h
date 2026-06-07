#pragma once

#include <stdint.h>

void trace_emit0(uint16_t event_id);
void trace_emit1(uint16_t event_id, uint32_t arg0);
void trace_emit2(uint16_t event_id, uint32_t arg0, uint32_t arg1);
void trace_emit3(uint16_t event_id, uint32_t arg0, uint32_t arg1,
                 uint32_t arg2);
void trace_emit4(uint16_t event_id, uint32_t arg0, uint32_t arg1,
                 uint32_t arg2, uint32_t arg3);

#ifdef ENABLE_SWO_TRACE
#include "trace_ids.h"
#define TRACE(event, format, ...) TRACE_CALL_##event(__VA_ARGS__)
#else
#define TRACE(event, format, ...) ((void)0)
#endif

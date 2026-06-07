#pragma once

#include <stdint.h>

#ifndef TRACE_ID_BASE
#define TRACE_ID_BASE 0U
#endif

void trace_emit0(uint16_t event_id);
void trace_emit1(uint16_t event_id, uint32_t arg0);
void trace_emit2(uint16_t event_id, uint32_t arg0, uint32_t arg1);
void trace_emit3(uint16_t event_id, uint32_t arg0, uint32_t arg1,
                 uint32_t arg2);
void trace_emit4(uint16_t event_id, uint32_t arg0, uint32_t arg1,
                 uint32_t arg2, uint32_t arg3);
void trace_abort_record(void);

#ifdef ENABLE_SWO_TRACE
#define TRACE_DETAIL_JOIN_INNER(a, b) a##b
#define TRACE_DETAIL_JOIN(a, b) TRACE_DETAIL_JOIN_INNER(a, b)
#define TRACE_DETAIL_FORMAT_PREFIX(count) TRACE_DETAIL_JOIN(trace_format_, count)
#define TRACE_DETAIL_FORMAT_BASE(count) TRACE_DETAIL_JOIN(TRACE_DETAIL_FORMAT_PREFIX(count), _)
#define TRACE_DETAIL_FORMAT_NAME(count, counter) \
    TRACE_DETAIL_JOIN(TRACE_DETAIL_FORMAT_BASE(count), counter)

#define TRACE_DETAIL_DECLARE_FORMAT(count, counter, format)                   \
    static const char TRACE_DETAIL_FORMAT_NAME(count, counter)[]              \
        __attribute__((section(".trace_fmt"), used, aligned(1))) = format

#define TRACE_DETAIL_ID(count, counter) \
    ((uint16_t)(TRACE_ID_BASE + \
                (uintptr_t)TRACE_DETAIL_FORMAT_NAME(count, counter)))

#define TRACE_DETAIL_CALL0(counter, format)                                   \
    do {                                                                       \
        TRACE_DETAIL_DECLARE_FORMAT(0, counter, format);                       \
        trace_emit0(TRACE_DETAIL_ID(0, counter));                              \
    } while (0)

#define TRACE_DETAIL_CALL1(counter, format, arg0)                              \
    do {                                                                       \
        TRACE_DETAIL_DECLARE_FORMAT(1, counter, format);                       \
        trace_emit1(TRACE_DETAIL_ID(1, counter), (uint32_t)(arg0));            \
    } while (0)

#define TRACE_DETAIL_CALL2(counter, format, arg0, arg1)                        \
    do {                                                                       \
        TRACE_DETAIL_DECLARE_FORMAT(2, counter, format);                       \
        trace_emit2(TRACE_DETAIL_ID(2, counter), (uint32_t)(arg0),             \
                    (uint32_t)(arg1));                                         \
    } while (0)

#define TRACE_DETAIL_CALL3(counter, format, arg0, arg1, arg2)                  \
    do {                                                                       \
        TRACE_DETAIL_DECLARE_FORMAT(3, counter, format);                       \
        trace_emit3(TRACE_DETAIL_ID(3, counter), (uint32_t)(arg0),             \
                    (uint32_t)(arg1), (uint32_t)(arg2));                       \
    } while (0)

#define TRACE_DETAIL_CALL4(counter, format, arg0, arg1, arg2, arg3)            \
    do {                                                                       \
        TRACE_DETAIL_DECLARE_FORMAT(4, counter, format);                       \
        trace_emit4(TRACE_DETAIL_ID(4, counter), (uint32_t)(arg0),             \
                    (uint32_t)(arg1), (uint32_t)(arg2), (uint32_t)(arg3));     \
    } while (0)

#define TRACE_DETAIL_SELECT(_1, _2, _3, _4, _5, name, ...) name
#define TRACE_DETAIL_DISPATCH(...)                                             \
    TRACE_DETAIL_SELECT(__VA_ARGS__, TRACE_DETAIL_CALL4, TRACE_DETAIL_CALL3,   \
                        TRACE_DETAIL_CALL2, TRACE_DETAIL_CALL1,                \
                        TRACE_DETAIL_CALL0)
#define TRACE(...) TRACE_DETAIL_DISPATCH(__VA_ARGS__)(__COUNTER__, __VA_ARGS__)
#else
#define TRACE(...) ((void)0)
#endif

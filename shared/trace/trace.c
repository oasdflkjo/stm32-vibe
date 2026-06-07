#include "trace/trace.h"
#include "hal/itm.h"
#include <stddef.h>

#define TRACE_SYNC_0 0xA5U
#define TRACE_SYNC_1 0x5AU
#define TRACE_VERSION 1U
#define TRACE_MAX_ARGS 4U

static uint8_t crc8_update(uint8_t crc, uint8_t value)
{
    crc ^= value;

    for (uint8_t bit = 0U; bit < 8U; bit++) {
        crc = (crc & 0x80U) != 0U
                  ? (uint8_t)((crc << 1U) ^ 0x07U)
                  : (uint8_t)(crc << 1U);
    }

    return crc;
}

static void trace_emit_record(uint16_t event_id, uint8_t arg_count,
                              const uint32_t *args)
{
    uint8_t crc = 0U;

    if (arg_count > TRACE_MAX_ARGS) {
        return;
    }

    itm_putchar(TRACE_SYNC_0);
    itm_putchar(TRACE_SYNC_1);

    const uint8_t header[] = {
        TRACE_VERSION,
        (uint8_t)(event_id & 0xFFU),
        (uint8_t)(event_id >> 8U),
        arg_count,
    };

    for (size_t i = 0U; i < sizeof(header); i++) {
        itm_putchar(header[i]);
        crc = crc8_update(crc, header[i]);
    }

    for (uint8_t arg_index = 0U; arg_index < arg_count; arg_index++) {
        uint32_t value = args[arg_index];

        for (uint8_t byte_index = 0U; byte_index < 4U; byte_index++) {
            uint8_t byte = (uint8_t)(value >> (byte_index * 8U));
            itm_putchar(byte);
            crc = crc8_update(crc, byte);
        }
    }

    itm_putchar(crc);
}

void trace_emit0(uint16_t event_id)
{
    trace_emit_record(event_id, 0U, NULL);
}

void trace_emit1(uint16_t event_id, uint32_t arg0)
{
    const uint32_t args[] = {arg0};
    trace_emit_record(event_id, 1U, args);
}

void trace_emit2(uint16_t event_id, uint32_t arg0, uint32_t arg1)
{
    const uint32_t args[] = {arg0, arg1};
    trace_emit_record(event_id, 2U, args);
}

void trace_emit3(uint16_t event_id, uint32_t arg0, uint32_t arg1,
                 uint32_t arg2)
{
    const uint32_t args[] = {arg0, arg1, arg2};
    trace_emit_record(event_id, 3U, args);
}

void trace_emit4(uint16_t event_id, uint32_t arg0, uint32_t arg1,
                 uint32_t arg2, uint32_t arg3)
{
    const uint32_t args[] = {arg0, arg1, arg2, arg3};
    trace_emit_record(event_id, 4U, args);
}

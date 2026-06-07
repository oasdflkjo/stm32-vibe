#pragma once

#include <stdint.h>

typedef enum {
    FAULT_TYPE_HARD = 1,
    FAULT_TYPE_MEMORY = 2,
    FAULT_TYPE_BUS = 3,
    FAULT_TYPE_USAGE = 4,
} fault_type_t;

typedef struct {
    fault_type_t type;
    uint32_t pc;
    uint32_t lr;
    uint32_t xpsr;
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t mmfar;
    uint32_t bfar;
} fault_report_t;

void fault_handlers_init(void);
void fault_report_emit(const fault_report_t *report);

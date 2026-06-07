#include "led_task.h"
#include "fault/fault.h"
#include "trace/trace.h"

#ifdef ENABLE_SWO_TRACE
#include "hal/itm.h"
#include "stm32l1xx.h"

#endif

int main(void)
{
#ifdef ENABLE_SWO_TRACE
    uint32_t toggle_count = 0U;

    itm_init(SystemCoreClock, TRACE_SWO_BAUD);
#endif
    fault_handlers_init();

    led_task_init();
    TRACE("SWO ready");

#ifdef ENABLE_FAULT_TEST
    __asm volatile(".short 0xDE00");
#endif

    while (1) {
        led_task_run();
#ifdef ENABLE_SWO_TRACE
        toggle_count++;
#endif
        TRACE("LED toggled count=%u", toggle_count);
    }
}

#include "led_task.h"
#include "fault/fault.h"
#include "hal/watchdog.h"
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

    if (!watchdog_init(WATCHDOG_TIMEOUT_MS)) {
        TRACE("WATCHDOG init failed");
        while (1) {
        }
    }
    TRACE("WATCHDOG started timeout_ms=%u", WATCHDOG_TIMEOUT_MS);

#ifdef ENABLE_WATCHDOG_TEST
    TRACE("WATCHDOG test waiting for reset");
    while (1) {
    }
#endif

    while (1) {
        led_task_run();
#ifdef ENABLE_SWO_TRACE
        toggle_count++;
#endif
        TRACE("LED toggled count=%u", toggle_count);
        watchdog_refresh();
    }
}

#include "led_task.h"
#include "trace/trace.h"

#ifdef ENABLE_SWO_TRACE
#include "hal/itm.h"
#include "stm32l1xx.h"

#endif

int main(void)
{
#ifdef ENABLE_SWO_TRACE
    uint32_t toggle_count = 0U;
#endif

    led_task_init();

#ifdef ENABLE_SWO_TRACE
    itm_init(SystemCoreClock, TRACE_SWO_BAUD);
#endif
    TRACE("SWO ready");

    while (1) {
        led_task_run();
#ifdef ENABLE_SWO_TRACE
        toggle_count++;
#endif
        TRACE("LED toggled count=%u", toggle_count);
    }
}

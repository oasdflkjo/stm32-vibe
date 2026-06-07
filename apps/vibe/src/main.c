#include "led_task.h"
#include "trace/trace.h"

#ifdef ENABLE_SWO_TRACE
#include "hal/itm.h"
#include "stm32l1xx.h"

#define SWO_BAUD 72000U
#endif

int main(void)
{
#ifdef ENABLE_SWO_TRACE
    uint32_t toggle_count = 0U;
#endif

    led_task_init();

#ifdef ENABLE_SWO_TRACE
    itm_init(SystemCoreClock, SWO_BAUD);
#endif
    TRACE(SWO_READY, "SWO ready");

    while (1) {
        led_task_run();
#ifdef ENABLE_SWO_TRACE
        toggle_count++;
#endif
        TRACE(LED_TOGGLED, "LED toggled count=%u", toggle_count);
    }
}

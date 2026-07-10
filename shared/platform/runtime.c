#include "platform/application.h"
#include "platform/boot_confirmation.h"
#include "platform/health_supervisor.h"
#include "platform/update_agent.h"

#include "boot/update_handoff.h"
#include "fault/fault.h"
#include "hal/gpio.h"
#include "hal/systick.h"
#include "hal/watchdog.h"
#include "trace/trace.h"

#include "stm32l1xx.h"

#ifdef ENABLE_SWO_TRACE
#include "hal/itm.h"
#endif

__attribute__((noreturn))
static void platform_request_update_reset(void)
{
    update_handoff_request();
    __DSB();
    NVIC_SystemReset();
}

static void platform_status_led_set(bool enabled)
{
    gpio_led_set(enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

int main(void)
{
    health_supervisor_t health_supervisor;
    boot_confirmation_t boot_confirmation;
    const app_services_t services = {
        .status_led_set = platform_status_led_set,
        .request_update_reset = platform_request_update_reset,
    };

#ifdef ENABLE_SWO_TRACE
    itm_init(SystemCoreClock, TRACE_SWO_BAUD);
#endif
    fault_handlers_init();
    gpio_led_init();
    systick_init();
    update_agent_init(platform_request_update_reset);

    if (app_init(&services) != APP_INIT_OK) {
        TRACE("APP init failed");
        while (1) {
        }
    }

    if (!watchdog_init(WATCHDOG_TIMEOUT_MS)) {
        TRACE("WATCHDOG init failed");
        while (1) {
        }
    }

    health_supervisor_init(&health_supervisor, WATCHDOG_TIMEOUT_MS / 2U);
    boot_confirmation_init(&boot_confirmation, BOOT_CONFIRM_STABILIZATION_MS,
                           platform_confirm_pending_boot);

    while (1) {
        uint32_t now_ms = systick_now_ms();

        update_agent_poll();
        app_process(now_ms);
        health_supervisor_report_application(&health_supervisor,
                                             app_is_healthy());
        health_supervisor_report_progress(&health_supervisor, now_ms);
        boot_confirmation_poll(&boot_confirmation, now_ms,
                               app_is_healthy());
        if (health_supervisor_may_feed_watchdog(&health_supervisor, now_ms)) {
            watchdog_refresh();
        }
    }
}

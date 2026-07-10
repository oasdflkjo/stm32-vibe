#include "platform/application.h"

#include "led_task.h"

static bool healthy;

app_init_result_t app_init(const app_services_t *services)
{
    if ((services == 0) || (services->status_led_set == 0) ||
        (services->request_update_reset == 0)) {
        return APP_INIT_FAILED;
    }

    healthy = false;
    led_task_init(services->status_led_set, 0U);
    healthy = true;
    return APP_INIT_OK;
}

void app_process(uint32_t now_ms)
{
    led_task_process(now_ms);
}

bool app_is_healthy(void)
{
    return healthy;
}

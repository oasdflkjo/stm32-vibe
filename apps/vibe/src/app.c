#include "platform/application.h"

#include "led_task.h"

#ifdef ENABLE_CAN_SMOKE_TEST
#include "hal/can.h"
#ifndef CAN_SMOKE_HEARTBEAT_ID
#define CAN_SMOKE_HEARTBEAT_ID 0x123U
#endif
#endif

static bool healthy;

#ifdef ENABLE_CAN_SMOKE_TEST
static uint32_t next_can_tx_ms;
static uint32_t can_counter;

static void can_smoke_process(uint32_t now_ms)
{
    can_frame_t received;
    if (can_receive(&received) == CAN_RESULT_OK) {
        received.id = 0x124U;
        (void)can_send(&received);
    }
    if ((int32_t)(now_ms - next_can_tx_ms) >= 0) {
        can_frame_t heartbeat = {
            .id = CAN_SMOKE_HEARTBEAT_ID,
            .dlc = 8U,
            .data = {'V', 'I', 'B', 'E',
                     (uint8_t)can_counter,
                     (uint8_t)(can_counter >> 8U),
                     (uint8_t)(can_counter >> 16U),
                     (uint8_t)(can_counter >> 24U)},
        };
        (void)can_send(&heartbeat);
        can_counter++;
        next_can_tx_ms += 500U;
    }
}
#endif

app_init_result_t app_init(const app_services_t *services)
{
    if ((services == 0) || (services->status_led_set == 0) ||
        (services->request_update_reset == 0)) {
        return APP_INIT_FAILED;
    }

    healthy = false;
    led_task_init(services->status_led_set, 0U);
#ifdef ENABLE_CAN_SMOKE_TEST
    if (can_init(500000U) != CAN_RESULT_OK) {
        return APP_INIT_FAILED;
    }
    can_counter = 0U;
    next_can_tx_ms = 500U;
#endif
    healthy = true;
    return APP_INIT_OK;
}

void app_process(uint32_t now_ms)
{
    led_task_process(now_ms);
#ifdef ENABLE_CAN_SMOKE_TEST
    can_smoke_process(now_ms);
#endif
}

bool app_is_healthy(void)
{
    return healthy;
}

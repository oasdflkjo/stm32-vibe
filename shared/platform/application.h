#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    APP_INIT_OK = 0,
    APP_INIT_FAILED = 1,
} app_init_result_t;

typedef struct {
    void (*status_led_set)(bool enabled);
    void (*request_update_reset)(void);
} app_services_t;

app_init_result_t app_init(const app_services_t *services);
void app_process(uint32_t now_ms);
bool app_is_healthy(void);

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef void (*led_task_output_fn_t)(bool enabled);

void led_task_init(led_task_output_fn_t output, uint32_t now_ms);
void led_task_process(uint32_t now_ms);

#pragma once

typedef void (*led_task_idle_fn_t)(void);

void led_task_init(void);
void led_task_run(void);
void led_task_run_with_idle(led_task_idle_fn_t idle);

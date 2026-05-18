#include "led_task.h"

int main(void)
{
    led_task_init();

    while (1) {
        led_task_run();
    }
}


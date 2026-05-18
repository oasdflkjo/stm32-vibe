#include <stdint.h>
#include "stm32l1xx.h"

#define APP_START_ADDR  0x08004000U

typedef void (*func_t)(void);

static void jump_to_app(void)
{
    uint32_t *app_vectors = (uint32_t *)APP_START_ADDR;

    /* Sanity check: top-of-stack must point into RAM */
    if ((app_vectors[0] & 0xFF000000U) != 0x20000000U) {
        /* No valid app — stay here */
        while (1);
    }

    /* Relocate vector table to app */
    SCB->VTOR = APP_START_ADDR;

    /* Set stack pointer and jump to app reset handler */
    __set_MSP(app_vectors[0]);
    func_t reset_handler = (func_t)app_vectors[1];
    reset_handler();
}

int main(void)
{
    jump_to_app();
    while (1);
}

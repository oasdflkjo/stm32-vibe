#include <stdint.h>
#include "app_validation.h"
#include "fault/fault.h"
#include "hal/itm.h"
#include "trace/trace.h"
#include "stm32l1xx.h"

__attribute__((naked, noreturn))
static void start_app(uint32_t stack_pointer, uint32_t reset_handler)
{
    (void)stack_pointer;
    (void)reset_handler;
    __asm volatile(
        "msr msp, r0\n"
        "movs r0, #0\n"
        "msr control, r0\n"
        "msr basepri, r0\n"
        "msr faultmask, r0\n"
        "cpsie i\n"
        "dsb\n"
        "isb\n"
        "bx r1\n"
    );
}

__attribute__((noreturn))
static void jump_to_app(uint32_t stack_pointer, uint32_t reset_handler)
{
    __disable_irq();

    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;

    for (uint32_t index = 0U;
         index < (sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0]));
         index++) {
        NVIC->ICER[index] = UINT32_MAX;
        NVIC->ICPR[index] = UINT32_MAX;
    }

    SCB->VTOR = APP_START_ADDR;
    __DSB();
    __ISB();
    start_app(stack_pointer, reset_handler);
}

int main(void)
{
    const uint32_t *app_vectors = (const uint32_t *)APP_START_ADDR;
    uint32_t stack_pointer = app_vectors[0];
    uint32_t reset_handler = app_vectors[1];
    uint32_t reset_cause = RCC->CSR;

    itm_init(SystemCoreClock, TRACE_SWO_BAUD);
    fault_handlers_init();
    TRACE("BOOT reset csr=%08X", reset_cause);
    RCC->CSR |= RCC_CSR_RMVF;
    TRACE("BOOT start");

    if (!app_vectors_are_valid(stack_pointer, reset_handler)) {
        TRACE("BOOT invalid app sp=%08X reset=%08X",
              stack_pointer, reset_handler);
        while (1) {
        }
    }

    TRACE("BOOT jump app");
    jump_to_app(stack_pointer, reset_handler);
}

#include "fault/fault.h"
#include "hal/itm.h"
#include "stm32l1xx.h"

#define RAM_START_ADDR 0x20000000U
#define RAM_END_ADDR   0x20014000U
#define EXCEPTION_FRAME_WORDS 8U

static fault_report_t fault_report;

static int exception_frame_is_valid(const uint32_t *frame)
{
    uintptr_t address = (uintptr_t)frame;
    uintptr_t last_word = address +
                          (EXCEPTION_FRAME_WORDS * sizeof(uint32_t));

    return ((address & 0x7U) == 0U) &&
           (address >= RAM_START_ADDR) &&
           (last_word <= RAM_END_ADDR);
}

__attribute__((noreturn))
void fault_handle_exception(const uint32_t *frame, fault_type_t type)
{
    uint32_t cfsr = SCB->CFSR;

    __disable_irq();
    itm_init(SystemCoreClock, TRACE_SWO_BAUD);

    fault_report.type = type;
    fault_report.pc = 0U;
    fault_report.lr = 0U;
    fault_report.xpsr = 0U;
    fault_report.cfsr = cfsr;
    fault_report.hfsr = SCB->HFSR;
    fault_report.mmfar =
        (cfsr & SCB_CFSR_MMARVALID_Msk) != 0U ? SCB->MMFAR : 0U;
    fault_report.bfar =
        (cfsr & SCB_CFSR_BFARVALID_Msk) != 0U ? SCB->BFAR : 0U;

    if (exception_frame_is_valid(frame)) {
        fault_report.lr = frame[5];
        fault_report.pc = frame[6];
        fault_report.xpsr = frame[7];
    }

    fault_report_emit(&fault_report);

    while (1) {
        __WFI();
    }
}

#define DEFINE_FAULT_HANDLER(name, fault_type)                                \
    __attribute__((naked, noreturn)) void name(void)                          \
    {                                                                          \
        __asm volatile(                                                        \
            "tst lr, #4\n"                                                     \
            "ite eq\n"                                                         \
            "mrseq r0, msp\n"                                                  \
            "mrsne r0, psp\n"                                                  \
            "movs r1, %0\n"                                                    \
            "b fault_handle_exception\n"                                       \
            :                                                                  \
            : "I"(fault_type));                                                \
    }

DEFINE_FAULT_HANDLER(HardFault_Handler, FAULT_TYPE_HARD)
DEFINE_FAULT_HANDLER(MemManage_Handler, FAULT_TYPE_MEMORY)
DEFINE_FAULT_HANDLER(BusFault_Handler, FAULT_TYPE_BUS)
DEFINE_FAULT_HANDLER(UsageFault_Handler, FAULT_TYPE_USAGE)

void fault_handlers_init(void)
{
    SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk |
                  SCB_SHCSR_BUSFAULTENA_Msk |
                  SCB_SHCSR_USGFAULTENA_Msk;
    SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;
}

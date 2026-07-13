#include "hal/itm.h"
#include "platform/device.h"

#define ITM_SEND_TIMEOUT 10000U

static uint8_t itm_ready;

void itm_init(uint32_t cpu_hz, uint32_t swo_baud)
{
    uint32_t prescaler;

    if ((cpu_hz == 0U) || (swo_baud == 0U)) {
        return;
    }
    prescaler = (cpu_hz / swo_baud) - 1U;
    if (((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) != 0U) &&
        ((ITM->TCR & ITM_TCR_ITMENA_Msk) != 0U) &&
        ((ITM->TER & 1U) != 0U) && (TPIU->ACPR == prescaler) &&
        (TPIU->SPPR == 2U)) {
        itm_ready = 1U;
        return;
    }

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DBGMCU->CR |= DBGMCU_CR_TRACE_IOEN;
    TPIU->ACPR = prescaler;
    TPIU->SPPR = 2U;
    TPIU->FFCR = 0x100U;
    ITM->LAR = 0xC5ACCE55UL;
    ITM->TCR = ITM_TCR_ITMENA_Msk | ITM_TCR_SYNCENA_Msk |
               ITM_TCR_DWTENA_Msk | (1UL << ITM_TCR_TRACEBUSID_Pos);
    ITM->TPR = 0U;
    ITM->TER = 1U;
    itm_ready = 1U;
}

void itm_putchar(uint8_t ch)
{
    uint32_t timeout = ITM_SEND_TIMEOUT;
    if (itm_ready == 0U) {
        return;
    }
    while ((ITM->PORT[0].u32 == 0U) && (timeout > 0U)) {
        timeout--;
    }
    if (timeout > 0U) {
        ITM->PORT[0].u8 = ch;
    }
}

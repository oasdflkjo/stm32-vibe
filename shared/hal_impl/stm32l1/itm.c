#include "hal/itm.h"
#include "stm32l1xx.h"

/*
 * SWO / ITM trace output for Cortex-M3 (STM32L1xx).
 *
 * itm_putchar waits for FIFO space with a bounded timeout. Bytes are dropped
 * if the trace path stays unavailable, so the application cannot stall forever.
 *
 * No printf / newlib stdio is used — no heap required.
 */

static uint8_t itm_ready = 0U;

#define ITM_SEND_TIMEOUT 10000U

void itm_init(uint32_t cpu_hz, uint32_t swo_baud)
{
    if ((cpu_hz == 0U) || (swo_baud == 0U)) {
        return;
    }

    /* 1. Enable the DWT / ITM trace clock */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* Route trace output to the single SWO pin. */
    DBGMCU->CR |= DBGMCU_CR_TRACE_IOEN;

    /* 2. Configure TPIU */
    TPIU->ACPR = (cpu_hz / swo_baud) - 1U; /* baud prescaler          */
    TPIU->SPPR = 2U;                        /* NRZ (async SWO) format  */
    TPIU->FFCR = 0x100U;                    /* enable continuous format */

    /* 3. Unlock and configure ITM */
    ITM->LAR = 0xC5ACCE55UL;              /* unlock write access      */
    ITM->TCR = ITM_TCR_ITMENA_Msk         /* enable ITM               */
             | ITM_TCR_SYNCENA_Msk        /* emit sync packets        */
             | ITM_TCR_DWTENA_Msk         /* forward DWT packets      */
             | (1UL << ITM_TCR_TRACEBUSID_Pos); /* bus ID = 1         */
    ITM->TPR = 0U;                        /* all ports unprivileged   */
    ITM->TER = 1U;                        /* enable stimulus port 0   */

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

void itm_puts(const char *s)
{
    while (*s != '\0') {
        itm_putchar((uint8_t)*s);
        s++;
    }
}

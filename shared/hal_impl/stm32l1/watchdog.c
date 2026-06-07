#include "hal/watchdog.h"
#include "stm32l1xx.h"

#define WATCHDOG_LSI_HZ          37000U
#define WATCHDOG_PRESCALER       64U
#define WATCHDOG_PRESCALER_BITS  IWDG_PR_PR_2
#define WATCHDOG_MAX_RELOAD      0x0FFFU
#define WATCHDOG_UPDATE_TIMEOUT  100000U

#define WATCHDOG_KEY_START        0xCCCCU
#define WATCHDOG_KEY_ENABLE_WRITE 0x5555U
#define WATCHDOG_KEY_REFRESH      0xAAAAU

int watchdog_init(uint32_t timeout_ms)
{
    uint32_t ticks;
    uint32_t update_timeout = WATCHDOG_UPDATE_TIMEOUT;

    if ((timeout_ms == 0U) ||
        (timeout_ms > ((WATCHDOG_MAX_RELOAD + 1U) *
                       WATCHDOG_PRESCALER * 1000U) / WATCHDOG_LSI_HZ)) {
        return 0;
    }

    ticks = ((timeout_ms * WATCHDOG_LSI_HZ) +
             (WATCHDOG_PRESCALER * 1000U) - 1U) /
            (WATCHDOG_PRESCALER * 1000U);

    DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_IWDG_STOP;

    IWDG->KR = WATCHDOG_KEY_START;
    IWDG->KR = WATCHDOG_KEY_ENABLE_WRITE;
    IWDG->PR = WATCHDOG_PRESCALER_BITS;
    IWDG->RLR = ticks - 1U;

    while ((IWDG->SR & (IWDG_SR_PVU | IWDG_SR_RVU)) != 0U) {
        if (update_timeout == 0U) {
            return 0;
        }
        update_timeout--;
    }

    watchdog_refresh();
    return 1;
}

void watchdog_refresh(void)
{
    IWDG->KR = WATCHDOG_KEY_REFRESH;
}

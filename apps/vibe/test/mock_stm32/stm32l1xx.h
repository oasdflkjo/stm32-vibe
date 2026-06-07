#pragma once

#include <stdint.h>

typedef struct {
    volatile uint32_t KR;
    volatile uint32_t PR;
    volatile uint32_t RLR;
    volatile uint32_t SR;
} IWDG_TypeDef;

typedef struct {
    volatile uint32_t IDCODE;
    volatile uint32_t CR;
    volatile uint32_t APB1FZ;
} DBGMCU_TypeDef;

extern IWDG_TypeDef mock_iwdg;
extern DBGMCU_TypeDef mock_dbgmcu;

#define IWDG   (&mock_iwdg)
#define DBGMCU (&mock_dbgmcu)

#define IWDG_PR_PR_2                    (1UL << 2)
#define IWDG_SR_PVU                     (1UL << 0)
#define IWDG_SR_RVU                     (1UL << 1)
#define DBGMCU_APB1_FZ_DBG_IWDG_STOP    (1UL << 12)

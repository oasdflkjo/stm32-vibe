#include "hal/can.h"
#include "platform/device.h"

#define CAN_AF 9U
#define CAN_RX_PIN 8U
#define CAN_TX_PIN 9U
#define CAN_TIME_QUANTA 16U
#define CAN_INIT_TIMEOUT 1000000U
#define CAN_DRAIN_TIMEOUT 1000000U

static can_status_t status;

static uint32_t can_apb1_clock_hz(void)
{
    static const uint8_t apb_prescaler_shift[] = {0U, 0U, 0U, 0U,
                                                  1U, 2U, 3U, 4U};
    uint32_t index = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;
    return SystemCoreClock >> apb_prescaler_shift[index];
}

static int wait_for_msr(uint32_t mask, uint32_t expected)
{
    uint32_t timeout = CAN_INIT_TIMEOUT;
    while ((CAN1->MSR & mask) != expected) {
        if (timeout-- == 0U) {
            return 0;
        }
    }
    return 1;
}

static void update_bus_state(void)
{
    status.bus_off = (CAN1->ESR & CAN_ESR_BOFF) != 0U ? 1U : 0U;
}

can_result_t can_init(uint32_t bitrate)
{
    uint32_t pclk = can_apb1_clock_hz();
    uint32_t divisor;
    uint32_t prescaler;

    if ((bitrate == 0U) || (bitrate > pclk / CAN_TIME_QUANTA)) {
        return CAN_RESULT_INVALID_ARGUMENT;
    }
    divisor = bitrate * CAN_TIME_QUANTA;
    if ((pclk % divisor) != 0U) {
        return CAN_RESULT_INVALID_ARGUMENT;
    }
    prescaler = pclk / divisor;
    if ((prescaler == 0U) || (prescaler > 1024U)) {
        return CAN_RESULT_INVALID_ARGUMENT;
    }

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_CAN1EN;
    (void)RCC->AHB1ENR;
    (void)RCC->APB1ENR;

    GPIOB->MODER &= ~((3U << (CAN_RX_PIN * 2U)) |
                      (3U << (CAN_TX_PIN * 2U)));
    GPIOB->MODER |= (2U << (CAN_RX_PIN * 2U)) |
                    (2U << (CAN_TX_PIN * 2U));
    GPIOB->OTYPER &= ~((1U << CAN_RX_PIN) | (1U << CAN_TX_PIN));
    GPIOB->OSPEEDR |= (3U << (CAN_RX_PIN * 2U)) |
                      (3U << (CAN_TX_PIN * 2U));
    GPIOB->PUPDR &= ~((3U << (CAN_RX_PIN * 2U)) |
                      (3U << (CAN_TX_PIN * 2U)));
    GPIOB->AFR[1] &= ~((0xFU << ((CAN_RX_PIN - 8U) * 4U)) |
                       (0xFU << ((CAN_TX_PIN - 8U) * 4U)));
    GPIOB->AFR[1] |= (CAN_AF << ((CAN_RX_PIN - 8U) * 4U)) |
                     (CAN_AF << ((CAN_TX_PIN - 8U) * 4U));

    CAN1->MCR = CAN_MCR_INRQ | CAN_MCR_ABOM | CAN_MCR_TXFP;
    if (!wait_for_msr(CAN_MSR_INAK, CAN_MSR_INAK)) {
        return CAN_RESULT_NOT_READY;
    }

    /* 16 time quanta: 1 sync + 13 BS1 + 2 BS2, sample point 87.5%. */
    CAN1->BTR = ((prescaler - 1U) << CAN_BTR_BRP_Pos) |
                (12U << CAN_BTR_TS1_Pos) | (1U << CAN_BTR_TS2_Pos);

    CAN1->FMR |= CAN_FMR_FINIT;
    CAN1->FA1R &= ~1U;
    CAN1->FM1R &= ~1U;
    CAN1->FS1R |= 1U;
    CAN1->FFA1R &= ~1U;
    CAN1->sFilterRegister[0].FR1 = 0U;
    CAN1->sFilterRegister[0].FR2 = 0U;
    CAN1->FA1R |= 1U;
    CAN1->FMR &= ~CAN_FMR_FINIT;

    CAN1->MCR &= ~CAN_MCR_INRQ;
    if (!wait_for_msr(CAN_MSR_INAK, 0U)) {
        return CAN_RESULT_NOT_READY;
    }

    status = (can_status_t){0};
    status.initialized = 1U;
    return CAN_RESULT_OK;
}

can_result_t can_send(const can_frame_t *frame)
{
    uint32_t mailbox;
    CAN_TxMailBox_TypeDef *tx;

    if ((frame == 0) || (frame->dlc > CAN_MAX_DATA_LEN) ||
        (frame->id > 0x7FFU)) {
        return CAN_RESULT_INVALID_ARGUMENT;
    }
    if (status.initialized == 0U) {
        return CAN_RESULT_NOT_READY;
    }
    update_bus_state();
    if (status.bus_off != 0U) {
        return CAN_RESULT_BUS_OFF;
    }
    if ((CAN1->TSR & CAN_TSR_TME) == 0U) {
        return CAN_RESULT_TX_FULL;
    }
    mailbox = (CAN1->TSR & CAN_TSR_TME0) != 0U ? 0U
              : (CAN1->TSR & CAN_TSR_TME1) != 0U ? 1U
                                                  : 2U;
    tx = &CAN1->sTxMailBox[mailbox];
    tx->TDTR = frame->dlc;
    tx->TDLR = (uint32_t)frame->data[0] |
               ((uint32_t)frame->data[1] << 8U) |
               ((uint32_t)frame->data[2] << 16U) |
               ((uint32_t)frame->data[3] << 24U);
    tx->TDHR = (uint32_t)frame->data[4] |
               ((uint32_t)frame->data[5] << 8U) |
               ((uint32_t)frame->data[6] << 16U) |
               ((uint32_t)frame->data[7] << 24U);
    tx->TIR = (frame->id << 21U) | CAN_TI0R_TXRQ;
    status.tx_count++;
    return CAN_RESULT_OK;
}

can_result_t can_receive(can_frame_t *frame)
{
    uint32_t rir;
    uint32_t low;
    uint32_t high;
    if (frame == 0) {
        return CAN_RESULT_INVALID_ARGUMENT;
    }
    if (status.initialized == 0U) {
        return CAN_RESULT_NOT_READY;
    }
    update_bus_state();
    if (status.bus_off != 0U) {
        return CAN_RESULT_BUS_OFF;
    }
    if ((CAN1->RF0R & CAN_RF0R_FMP0) == 0U) {
        return CAN_RESULT_RX_EMPTY;
    }
    rir = CAN1->sFIFOMailBox[0].RIR;
    frame->id = (rir & CAN_RI0R_IDE) != 0U
                    ? (rir >> 3U) & 0x1FFFFFFFU
                    : (rir >> 21U) & 0x7FFU;
    frame->dlc = (uint8_t)(CAN1->sFIFOMailBox[0].RDTR & 0xFU);
    low = CAN1->sFIFOMailBox[0].RDLR;
    high = CAN1->sFIFOMailBox[0].RDHR;
    for (uint32_t index = 0U; index < 4U; index++) {
        frame->data[index] = (uint8_t)(low >> (index * 8U));
        frame->data[index + 4U] = (uint8_t)(high >> (index * 8U));
    }
    CAN1->RF0R |= CAN_RF0R_RFOM0;
    status.rx_count++;
    return CAN_RESULT_OK;
}

can_result_t can_drain_tx(void)
{
    uint32_t timeout = CAN_DRAIN_TIMEOUT;
    if (status.initialized == 0U) {
        return CAN_RESULT_NOT_READY;
    }
    while ((CAN1->TSR & CAN_TSR_TME) != CAN_TSR_TME) {
        update_bus_state();
        if (status.bus_off != 0U) {
            return CAN_RESULT_BUS_OFF;
        }
        if (timeout-- == 0U) {
            return CAN_RESULT_TX_FULL;
        }
    }
    return CAN_RESULT_OK;
}

can_status_t can_get_status(void)
{
    if (status.initialized != 0U) {
        update_bus_state();
    }
    return status;
}

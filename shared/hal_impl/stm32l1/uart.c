#include "hal/uart.h"
#include "stm32l1xx.h"

#define UART_TX_PIN 2U
#define UART_RX_PIN 3U
#define UART_AF_USART2 7U

static uint8_t initialized;

static uint32_t uart_apb1_clock_hz(void)
{
    static const uint8_t apb_prescaler_shift[] = {0U, 0U, 0U, 0U,
                                                  1U, 2U, 3U, 4U};
    uint32_t index = (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos;

    return SystemCoreClock >> apb_prescaler_shift[index];
}

uart_result_t uart_init(uint32_t baudrate)
{
    uint32_t brr;

    if (baudrate == 0U) {
        return UART_RESULT_INVALID_ARGUMENT;
    }

    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    (void)RCC->AHBENR;
    (void)RCC->APB1ENR;

    GPIOA->MODER &= ~(GPIO_MODER_MODER2_Msk | GPIO_MODER_MODER3_Msk);
    GPIOA->MODER |= GPIO_MODER_MODER2_1 | GPIO_MODER_MODER3_1;
    GPIOA->OTYPER &= ~((1U << UART_TX_PIN) | (1U << UART_RX_PIN));
    GPIOA->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEEDR2_Msk |
                        GPIO_OSPEEDR_OSPEEDR3_Msk);
    GPIOA->OSPEEDR |= GPIO_OSPEEDR_OSPEEDR2_Msk |
                      GPIO_OSPEEDR_OSPEEDR3_Msk;
    GPIOA->PUPDR &= ~(GPIO_PUPDR_PUPDR2_Msk | GPIO_PUPDR_PUPDR3_Msk);
    GPIOA->PUPDR |= GPIO_PUPDR_PUPDR3_0;
    GPIOA->AFR[0] &= ~(GPIO_AFRL_AFSEL2_Msk | GPIO_AFRL_AFSEL3_Msk);
    GPIOA->AFR[0] |= (UART_AF_USART2 << GPIO_AFRL_AFSEL2_Pos) |
                     (UART_AF_USART2 << GPIO_AFRL_AFSEL3_Pos);

    USART2->CR1 = 0U;
    USART2->CR2 = 0U;
    USART2->CR3 = 0U;
    brr = (uart_apb1_clock_hz() + (baudrate / 2U)) / baudrate;
    USART2->BRR = brr;
    USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;

    initialized = 1U;
    return UART_RESULT_OK;
}

uart_result_t uart_receive_byte(uint8_t *byte)
{
    if (byte == 0) {
        return UART_RESULT_INVALID_ARGUMENT;
    }

    if (initialized == 0U) {
        return UART_RESULT_NOT_READY;
    }

    if ((USART2->SR & USART_SR_RXNE) == 0U) {
        return UART_RESULT_RX_EMPTY;
    }

    *byte = (uint8_t)USART2->DR;
    return UART_RESULT_OK;
}

uart_result_t uart_send(const uint8_t *data, size_t len)
{
    if ((len != 0U) && (data == 0)) {
        return UART_RESULT_INVALID_ARGUMENT;
    }

    if (initialized == 0U) {
        return UART_RESULT_NOT_READY;
    }

    for (size_t index = 0U; index < len; index++) {
        while ((USART2->SR & USART_SR_TXE) == 0U) {
        }
        USART2->DR = data[index];
    }

    return UART_RESULT_OK;
}

uart_result_t uart_drain_tx(void)
{
    if (initialized == 0U) {
        return UART_RESULT_NOT_READY;
    }

    while ((USART2->SR & USART_SR_TC) == 0U) {
    }

    return UART_RESULT_OK;
}

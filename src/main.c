#include "stm32l1xx.h"

#define LED_PIN 5U

static void delay(void)
{
  for (volatile uint32_t i = 0; i < 500000U; i++) {
    __NOP();
  }
}

int main(void)
{
  RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
  (void)RCC->AHBENR;

  GPIOA->MODER &= ~GPIO_MODER_MODER5_Msk;
  GPIOA->MODER |= GPIO_MODER_MODER5_0;
  GPIOA->OTYPER &= ~(1U << LED_PIN);
  GPIOA->PUPDR &= ~GPIO_PUPDR_PUPDR5_Msk;

  while (1) {
    GPIOA->BSRR = GPIO_BSRR_BS_5;
    delay();
    GPIOA->BSRR = GPIO_BSRR_BR_5;
    delay();
  }
}

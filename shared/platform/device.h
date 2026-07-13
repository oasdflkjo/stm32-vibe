#pragma once

#if defined(STM32F446xx)
#include "stm32f4xx.h"
#elif defined(STM32L152xE)
#include "stm32l1xx.h"
#else
#error Unsupported STM32 device
#endif

BOARD_NAME := ST NUCLEO-F446RE
BOARD_MCU := STM32F446RE
BOARD_HAL_IMPL := stm32f4

CPU_HZ := 16000000
SWO_BAUD := 72000

MCU_FLAGS := -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
BOARD_DEFINES := -DSTM32F446xx

CMSIS_DEVICE_DIR := $(REPO_ROOT)/vendor/cmsis_device_f4
BOARD_INCLUDES := \
  -I$(REPO_ROOT)/vendor/cmsis-core/CMSIS/Core/Include \
  -I$(CMSIS_DEVICE_DIR)/Include

DEVICE_SYSTEM := $(CMSIS_DEVICE_DIR)/Source/Templates/system_stm32f4xx.c
DEVICE_STARTUP := $(CMSIS_DEVICE_DIR)/Source/Templates/gcc/startup_stm32f446xx.s
DEVICE_SYSTEM_OBJ := system_stm32f4xx.o
DEVICE_STARTUP_OBJ := startup_stm32f446xx.o

HAL_IMPL_DIR := $(REPO_ROOT)/shared/hal_impl/stm32f4
FAULT_IMPL_SRC := $(REPO_ROOT)/shared/fault/fault_stm32f4.c

OPENOCD_TARGET := stm32f4x
OPENOCD_TPIU := stm32f4x.tpiu

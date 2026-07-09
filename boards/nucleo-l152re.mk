BOARD_NAME := ST NUCLEO-L152RE
BOARD_MCU := STM32L152RE
BOARD_HAL_IMPL := stm32l1

CPU_HZ := 2097000
SWO_BAUD := 72000

MCU_FLAGS := -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
BOARD_DEFINES := -DSTM32L152xE

CMSIS_DEVICE_DIR := $(REPO_ROOT)/vendor/cmsis_device_l1
BOARD_INCLUDES := \
  -I$(REPO_ROOT)/vendor/cmsis-core/CMSIS/Core/Include \
  -I$(CMSIS_DEVICE_DIR)/Include

DEVICE_SYSTEM := $(CMSIS_DEVICE_DIR)/Source/Templates/system_stm32l1xx.c
DEVICE_STARTUP := $(CMSIS_DEVICE_DIR)/Source/Templates/gcc/startup_stm32l152xe.s
DEVICE_SYSTEM_OBJ := system_stm32l1xx.o
DEVICE_STARTUP_OBJ := startup_stm32l152xe.o

HAL_IMPL_DIR := $(REPO_ROOT)/shared/hal_impl/stm32l1
FAULT_IMPL_SRC := $(REPO_ROOT)/shared/fault/fault_stm32l1.c

OPENOCD_TARGET := stm32l1
OPENOCD_TPIU := stm32l1.tpiu

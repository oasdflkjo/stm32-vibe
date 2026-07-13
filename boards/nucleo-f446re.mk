BOARD_NAME := ST NUCLEO-F446RE
BOARD_MCU := STM32F446RE
BOARD_HAL_IMPL := stm32f4

# Sectors 0-1 hold the bootloader; sectors 2-3 are independent boot-state
# copies. Slots A/B occupy sectors 5/6, leaving sectors 4 and 7 reserved.
BOOTLOADER_FLASH_ADDR := 0x08000000
BOOTLOADER_FLASH_SIZE := 0x00008000
BOOT_STATE_FLASH_ADDR := 0x08008000
BOOT_STATE_FLASH_SIZE := 0x00008000
APP_SLOT_SIZE := 0x00020000
APP_SLOT_A_FLASH_ADDR := 0x08020000
APP_SLOT_B_FLASH_ADDR := 0x08040000
RAM_LENGTH := 126K

CPU_HZ := 16000000
SWO_BAUD := 72000

MCU_FLAGS := -mcpu=cortex-m4 -mthumb -mfloat-abi=soft
BOARD_DEFINES := -DSTM32F446xx -DBOARD_HAS_CAN=1 -DUPDATE_CAN_NODE_ID=1U

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
BOOT_FLASH_IMPL_SRC := src/boot_flash_stm32f4.c
CAN_IMPL_SRC := $(HAL_IMPL_DIR)/can.c
BOOT_CAN_SRC := src/update_can_loop.c
PIC_STARTUP_SRC := $(REPO_ROOT)/shared/platform/pic_startup_stm32f4.s

OPENOCD_TARGET := stm32f4x
OPENOCD_TPIU := stm32f4x.tpiu

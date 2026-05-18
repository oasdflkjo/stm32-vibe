PROJECT := bootloader
BUILD_DIR := build

REPO_ROOT := ..
VENDOR := $(REPO_ROOT)/vendor

TOOLCHAIN_PREFIX ?= arm-none-eabi-
CC      := $(TOOLCHAIN_PREFIX)gcc
OBJCOPY := $(TOOLCHAIN_PREFIX)objcopy
SIZE    := $(TOOLCHAIN_PREFIX)size
GDB     ?= $(TOOLCHAIN_PREFIX)gdb
ST_FLASH := st-flash
ST_INFO  := st-info
OPENOCD  := openocd

MCU_FLAGS := -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
DEFINES   := -DSTM32L152xE
INCLUDES  := \
  -I$(VENDOR)/cmsis-core/CMSIS/Core/Include \
  -I$(VENDOR)/cmsis_device_l1/Include

CFLAGS  := $(MCU_FLAGS) $(DEFINES) $(INCLUDES) -std=c11 -Wall -Wextra -Werror -Os -ffunction-sections -fdata-sections
ASFLAGS := $(MCU_FLAGS)
LDFLAGS := $(MCU_FLAGS) -Tlinker.ld -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(PROJECT).map --specs=nano.specs --specs=nosys.specs

SRCS := \
  src/main.c \
  src/syscalls.c

DEVICE_SYSTEM  := $(VENDOR)/cmsis_device_l1/Source/Templates/system_stm32l1xx.c
DEVICE_STARTUP := $(VENDOR)/cmsis_device_l1/Source/Templates/gcc/startup_stm32l152xe.s

OBJS := \
  $(patsubst src/%,$(BUILD_DIR)/%,$(SRCS:.c=.o)) \
  $(BUILD_DIR)/system_stm32l1xx.o \
  $(BUILD_DIR)/startup_stm32l152xe.o

ELF := $(BUILD_DIR)/$(PROJECT).elf
BIN := $(BUILD_DIR)/$(PROJECT).bin

.PHONY: firmware clean size flash probe reset erase debug-server gdb

firmware: $(ELF) $(BIN) size

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/%.s | $(BUILD_DIR)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/system_stm32l1xx.o: $(DEVICE_SYSTEM) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/startup_stm32l152xe.o: $(DEVICE_STARTUP) | $(BUILD_DIR)
	$(CC) $(ASFLAGS) -c $< -o $@

$(ELF): $(OBJS) linker.ld
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

size: $(ELF)
	$(SIZE) $<

flash: $(BIN)
	$(ST_FLASH) --reset write $(BIN) 0x08000000

probe:
	$(ST_INFO) --probe

reset:
	$(ST_FLASH) reset

erase:
	$(ST_FLASH) erase

debug-server:
	$(OPENOCD) -f interface/stlink.cfg -f target/stm32l1.cfg

gdb: $(ELF)
	$(GDB) $(ELF)

clean:
	rm -rf $(BUILD_DIR)

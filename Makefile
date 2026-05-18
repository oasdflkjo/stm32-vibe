PROJECT := stm32-vibe
BUILD_DIR := build
CONTAINER_ENGINE ?= podman
CONTAINER_IMAGE ?= stm32-vibe-build
CONTAINER_VOLUME_SUFFIX ?= :Z

export CCACHE_DISABLE ?= 1

TOOLCHAIN_PREFIX ?= arm-none-eabi-
CC := $(TOOLCHAIN_PREFIX)gcc
OBJCOPY := $(TOOLCHAIN_PREFIX)objcopy
SIZE := $(TOOLCHAIN_PREFIX)size
ST_FLASH := st-flash
ST_INFO := st-info
OPENOCD := openocd
GDB ?= $(TOOLCHAIN_PREFIX)gdb

MCU_FLAGS := -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
DEFINES := -DSTM32L152xE
INCLUDES := \
  -Ivendor/cmsis-core/CMSIS/Core/Include \
  -Ivendor/cmsis_device_l1/Include

CFLAGS := $(MCU_FLAGS) $(DEFINES) $(INCLUDES) -std=c11 -Wall -Wextra -Werror -Os -ffunction-sections -fdata-sections
ASFLAGS := $(MCU_FLAGS)
LDFLAGS := $(MCU_FLAGS) -Tlinker.ld -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(PROJECT).map --specs=nano.specs --specs=nosys.specs

APP_SRCS := \
  src/main.c \
  src/syscalls.c

DEVICE_SYSTEM := vendor/cmsis_device_l1/Source/Templates/system_stm32l1xx.c
DEVICE_STARTUP := vendor/cmsis_device_l1/Source/Templates/gcc/startup_stm32l152xe.s

OBJS := \
  $(patsubst src/%,$(BUILD_DIR)/%,$(APP_SRCS:.c=.o)) \
  $(BUILD_DIR)/system_stm32l1xx.o \
  $(BUILD_DIR)/startup_stm32l152xe.o

ELF := $(BUILD_DIR)/$(PROJECT).elf
BIN := $(BUILD_DIR)/$(PROJECT).bin

.PHONY: all clean container-build container-shell containerized-build debug-server erase firmware flash gdb probe reset size

all: containerized-build

firmware: $(ELF) $(BIN) size

container-build:
	$(CONTAINER_ENGINE) build -t $(CONTAINER_IMAGE) -f Containerfile .

containerized-build: container-build
	$(CONTAINER_ENGINE) run --rm -v "$(CURDIR):/workspace$(CONTAINER_VOLUME_SUFFIX)" -w /workspace $(CONTAINER_IMAGE) make clean firmware

container-shell: container-build
	$(CONTAINER_ENGINE) run --rm -it -v "$(CURDIR):/workspace$(CONTAINER_VOLUME_SUFFIX)" -w /workspace $(CONTAINER_IMAGE) bash

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
	@echo "If LD2 does not start blinking, press the board RESET button once."

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

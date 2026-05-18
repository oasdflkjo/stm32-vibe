PROJECT := vibe
BUILD_DIR := build
TEST_DIR := build/test

REPO_ROOT := ../..
VENDOR := $(REPO_ROOT)/vendor
SHARED := $(REPO_ROOT)/shared

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
  -Isrc \
  -I$(SHARED) \
  -I$(VENDOR)/cmsis-core/CMSIS/Core/Include \
  -I$(VENDOR)/cmsis_device_l1/Include

CFLAGS  := $(MCU_FLAGS) $(DEFINES) $(INCLUDES) -std=c11 -Wall -Wextra -Werror -Os -ffunction-sections -fdata-sections
ASFLAGS := $(MCU_FLAGS)
LDFLAGS := $(MCU_FLAGS) -Tlinker.ld -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(PROJECT).map --specs=nano.specs --specs=nosys.specs

APP_SRCS := \
  src/main.c \
  src/syscalls.c \
  src/led_task.c \
  $(SHARED)/hal_impl/stm32l1/gpio.c \
  $(SHARED)/hal_impl/stm32l1/systick.c

DEVICE_SYSTEM  := $(VENDOR)/cmsis_device_l1/Source/Templates/system_stm32l1xx.c
DEVICE_STARTUP := $(VENDOR)/cmsis_device_l1/Source/Templates/gcc/startup_stm32l152xe.s

OBJS := \
  $(patsubst $(REPO_ROOT)/%,$(BUILD_DIR)/%.o,$(patsubst src/%,$(BUILD_DIR)/%.o,$(APP_SRCS))) \
  $(BUILD_DIR)/system_stm32l1xx.o \
  $(BUILD_DIR)/startup_stm32l152xe.o

ELF := $(BUILD_DIR)/$(PROJECT).elf
BIN := $(BUILD_DIR)/$(PROJECT).bin

.PHONY: firmware test clean size flash probe reset erase debug-server gdb

firmware: $(ELF) $(BIN) size

# ── Unit tests (host gcc, mock HAL) ──────────────────────────────────────────
UNITY := $(REPO_ROOT)/vendor/unity/src

TEST_SRCS := \
  test/test_led_task.c \
  src/led_task.c \
  $(SHARED)/hal_impl/mock/gpio.c \
  $(SHARED)/hal_impl/mock/systick.c \
  $(UNITY)/unity.c

TEST_BIN := $(TEST_DIR)/test_led_task

TEST_CFLAGS := -Isrc -I$(SHARED) -I$(UNITY) -std=c11 -Wall -Wextra -Werror -g

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRCS) | $(TEST_DIR)
	gcc $(TEST_CFLAGS) $^ -o $@

$(TEST_DIR):
	mkdir -p $@

# ── Firmware build rules ──────────────────────────────────────────────────────
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

$(BUILD_DIR)/%.c.o: $(REPO_ROOT)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(ELF): $(OBJS) linker.ld
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

size: $(ELF)
	$(SIZE) $<

flash: $(BIN)
	$(ST_FLASH) --reset write $(BIN) 0x08004000

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

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
PYTHON   ?= python3

SWO_CLOCK ?= 2097000
SWO_BAUD  ?= 72000
SWO_LOG   ?= /tmp/$(PROJECT)_swo.log
SWO_BUILD_DIR ?= build/swo
SWO_TRACE_MAP ?= $(SWO_BUILD_DIR)/trace_map.json

TRACE_GEN_DIR := $(BUILD_DIR)/generated
TRACE_HEADER  := $(TRACE_GEN_DIR)/trace_ids.h
TRACE_MAP     := $(BUILD_DIR)/trace_map.json
TRACE_GENERATOR := $(REPO_ROOT)/tools/gen_trace.py
TRACE_DECODER   := $(REPO_ROOT)/tools/decode_trace.py

MCU_FLAGS := -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
DEFINES   := -DSTM32L152xE
INCLUDES  := \
  -Isrc \
  -I$(SHARED) \
  -I$(TRACE_GEN_DIR) \
  -I$(VENDOR)/cmsis-core/CMSIS/Core/Include \
  -I$(VENDOR)/cmsis_device_l1/Include

CFLAGS  := $(MCU_FLAGS) $(DEFINES) $(INCLUDES) -std=c11 -Wall -Wextra -Werror -Os -ffunction-sections -fdata-sections
ASFLAGS := $(MCU_FLAGS)
LDFLAGS := $(MCU_FLAGS) -Tlinker.ld -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(PROJECT).map --specs=nano.specs --specs=nosys.specs

APP_LOCAL_SRCS := \
  src/main.c \
  src/syscalls.c \
  src/led_task.c

HAL_SRCS := \
  $(SHARED)/hal_impl/stm32l1/gpio.c \
  $(SHARED)/hal_impl/stm32l1/systick.c \
  $(SHARED)/hal_impl/stm32l1/itm.c \
  $(SHARED)/trace/trace.c

LOCAL_OBJS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(APP_LOCAL_SRCS))
HAL_OBJS   := $(patsubst $(SHARED)/%.c,$(BUILD_DIR)/shared/%.o,$(HAL_SRCS))

DEVICE_SYSTEM  := $(VENDOR)/cmsis_device_l1/Source/Templates/system_stm32l1xx.c
DEVICE_STARTUP := $(VENDOR)/cmsis_device_l1/Source/Templates/gcc/startup_stm32l152xe.s

OBJS := \
  $(LOCAL_OBJS) \
  $(HAL_OBJS) \
  $(BUILD_DIR)/system_stm32l1xx.o \
  $(BUILD_DIR)/startup_stm32l152xe.o

ELF := $(BUILD_DIR)/$(PROJECT).elf
BIN := $(BUILD_DIR)/$(PROJECT).bin

.PHONY: firmware trace-generate test clean size flash flash-swo probe reset erase debug-server gdb swo-trace swo-print

firmware: $(ELF) $(BIN) size

trace-generate:
	@mkdir -p $(TRACE_GEN_DIR)
	$(PYTHON) $(TRACE_GENERATOR) \
	  --header $(TRACE_HEADER) \
	  --map $(TRACE_MAP) \
	  $(APP_LOCAL_SRCS)

# ── Unit tests (host gcc, mock HAL) ──────────────────────────────────────────
UNITY := $(REPO_ROOT)/vendor/unity/src

TEST_LED_SRCS := \
  test/test_led_task.c \
  src/led_task.c \
  $(SHARED)/hal_impl/mock/gpio.c \
  $(SHARED)/hal_impl/mock/systick.c \
  $(UNITY)/unity.c

TEST_TRACE_SRCS := \
  test/test_trace.c \
  $(SHARED)/trace/trace.c \
  $(SHARED)/hal_impl/mock/itm.c \
  $(UNITY)/unity.c

TEST_LED_BIN := $(TEST_DIR)/test_led_task
TEST_TRACE_BIN := $(TEST_DIR)/test_trace

TEST_CFLAGS := -Isrc -I$(SHARED) -I$(UNITY) -std=c11 -Wall -Wextra -Werror -g

test: $(TEST_LED_BIN) $(TEST_TRACE_BIN)
	./$(TEST_LED_BIN)
	./$(TEST_TRACE_BIN)
	cd $(REPO_ROOT) && $(PYTHON) -m unittest tools.test_trace_tools

$(TEST_LED_BIN): $(TEST_LED_SRCS) | $(TEST_DIR)
	gcc $(TEST_CFLAGS) $^ -o $@

$(TEST_TRACE_BIN): $(TEST_TRACE_SRCS) | $(TEST_DIR)
	gcc $(TEST_CFLAGS) $^ -o $@

$(TEST_DIR):
	mkdir -p $@

# ── Firmware build rules ──────────────────────────────────────────────────────
$(OBJS): | trace-generate

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/%.s | $(BUILD_DIR)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BUILD_DIR)/shared/%.o: $(SHARED)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

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
	$(ST_FLASH) --reset write $(BIN) 0x08004000

flash-swo:
	$(MAKE) -B flash \
	  BUILD_DIR=$(SWO_BUILD_DIR) \
	  DEFINES="$(DEFINES) -DENABLE_SWO_TRACE"

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

swo-trace:
	$(OPENOCD) -f interface/stlink.cfg -f target/stm32l1.cfg \
	  -c "init" \
	  -c "stm32l1.tpiu configure -protocol uart -traceclk $(SWO_CLOCK) -pin-freq $(SWO_BAUD) -formatter 0 -output $(SWO_LOG)" \
	  -c "stm32l1.tpiu enable" \
	  -c "reset run"

swo-print:
	$(PYTHON) $(TRACE_DECODER) --map $(SWO_TRACE_MAP) --input $(SWO_LOG) --follow

clean:
	rm -rf $(BUILD_DIR)

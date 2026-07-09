PROJECT := vibe
BUILD_DIR := build
TEST_DIR := build/test

REPO_ROOT := ../..
include $(REPO_ROOT)/config.mk

VENDOR := $(REPO_ROOT)/vendor
SHARED := $(REPO_ROOT)/shared

TOOLCHAIN_PREFIX ?= arm-none-eabi-
CC      := $(TOOLCHAIN_PREFIX)gcc
OBJCOPY := $(TOOLCHAIN_PREFIX)objcopy
NM      := $(TOOLCHAIN_PREFIX)nm
SIZE    := $(TOOLCHAIN_PREFIX)size
GDB     ?= $(TOOLCHAIN_PREFIX)gdb
ST_FLASH := st-flash
ST_INFO  := st-info
OPENOCD  := openocd
PYTHON   ?= python3

SWO_CLOCK ?= $(CPU_HZ)
SWO_LOG   ?= /tmp/$(PROJECT)_swo.log
SWO_BUILD_DIR ?= build/swo

TRACE_MAP     := $(BUILD_DIR)/trace_map.json
TRACE_EXTRACTOR := $(REPO_ROOT)/tools/extract_trace_map.py
IMAGE_FINALIZER := $(REPO_ROOT)/tools/finalize_image.py

DEFINES   := $(BOARD_DEFINES) -DTRACE_SWO_BAUD=$(SWO_BAUD)U -DWATCHDOG_TIMEOUT_MS=$(WATCHDOG_TIMEOUT_MS)U
INCLUDES  := \
  -Isrc \
  -I$(SHARED) \
  $(BOARD_INCLUDES)

CFLAGS  := $(MCU_FLAGS) $(DEFINES) $(INCLUDES) -std=c11 -Wall -Wextra -Werror -Os -g3 -ffunction-sections -fdata-sections
ASFLAGS := $(MCU_FLAGS) -g3
LDFLAGS := $(MCU_FLAGS) -Tlinker.ld -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(PROJECT).map --specs=nano.specs --specs=nosys.specs

APP_LOCAL_SRCS := \
  src/main.c \
  src/app_manifest.c \
  src/led_task.c

HAL_SRCS := \
  $(SHARED)/fault/fault_report.c \
  $(FAULT_IMPL_SRC) \
  $(SHARED)/libc/syscalls.c \
  $(HAL_IMPL_DIR)/critical.c \
  $(HAL_IMPL_DIR)/gpio.c \
  $(HAL_IMPL_DIR)/systick.c \
  $(HAL_IMPL_DIR)/itm.c \
  $(HAL_IMPL_DIR)/watchdog.c \
  $(SHARED)/trace/trace.c

LOCAL_OBJS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(APP_LOCAL_SRCS))
HAL_OBJS   := $(patsubst $(SHARED)/%.c,$(BUILD_DIR)/shared/%.o,$(HAL_SRCS))

OBJS := \
  $(LOCAL_OBJS) \
  $(HAL_OBJS) \
  $(BUILD_DIR)/$(DEVICE_SYSTEM_OBJ) \
  $(BUILD_DIR)/$(DEVICE_STARTUP_OBJ)

ELF := $(BUILD_DIR)/$(PROJECT).elf
BIN := $(BUILD_DIR)/$(PROJECT).bin

.PHONY: firmware test clean size flash flash-swo flash-fault-test flash-watchdog-test probe reset erase debug-server gdb swo-trace

firmware: $(ELF) $(BIN) $(TRACE_MAP) size

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
  $(SHARED)/hal_impl/mock/critical.c \
  $(SHARED)/hal_impl/mock/itm.c \
  $(UNITY)/unity.c

TEST_FAULT_SRCS := \
  test/test_fault_report.c \
  $(SHARED)/fault/fault_report.c \
  $(SHARED)/trace/trace.c \
  $(SHARED)/hal_impl/mock/critical.c \
  $(SHARED)/hal_impl/mock/itm.c \
  $(UNITY)/unity.c

TEST_WATCHDOG_SRCS := \
  test/test_watchdog.c \
  $(SHARED)/hal_impl/stm32l1/watchdog.c \
  $(UNITY)/unity.c

TEST_CAN_SRCS := \
  test/test_can.c \
  $(SHARED)/hal_impl/mock/can.c \
  $(UNITY)/unity.c

TEST_LED_BIN := $(TEST_DIR)/test_led_task
TEST_TRACE_BIN := $(TEST_DIR)/test_trace
TEST_FAULT_BIN := $(TEST_DIR)/test_fault_report
TEST_WATCHDOG_BIN := $(TEST_DIR)/test_watchdog
TEST_CAN_BIN := $(TEST_DIR)/test_can

TEST_CFLAGS := -Isrc -I$(SHARED) -I$(UNITY) -std=c11 -Wall -Wextra -Werror -g

test: $(TEST_LED_BIN) $(TEST_TRACE_BIN) $(TEST_FAULT_BIN) $(TEST_WATCHDOG_BIN) $(TEST_CAN_BIN)
	./$(TEST_LED_BIN)
	./$(TEST_TRACE_BIN)
	./$(TEST_FAULT_BIN)
	./$(TEST_WATCHDOG_BIN)
	./$(TEST_CAN_BIN)
	cd $(REPO_ROOT) && $(PYTHON) -m unittest tools.test_trace_tools

$(TEST_LED_BIN): $(TEST_LED_SRCS) | $(TEST_DIR)
	gcc $(TEST_CFLAGS) $^ -o $@

$(TEST_TRACE_BIN): $(TEST_TRACE_SRCS) | $(TEST_DIR)
	gcc $(TEST_CFLAGS) $^ -o $@

$(TEST_FAULT_BIN): $(TEST_FAULT_SRCS) | $(TEST_DIR)
	gcc $(TEST_CFLAGS) -DENABLE_SWO_TRACE $^ -o $@

$(TEST_WATCHDOG_BIN): $(TEST_WATCHDOG_SRCS) | $(TEST_DIR)
	gcc -Itest/mock_stm32 $(TEST_CFLAGS) $^ -o $@

$(TEST_CAN_BIN): $(TEST_CAN_SRCS) | $(TEST_DIR)
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

$(BUILD_DIR)/shared/%.o: $(SHARED)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/$(DEVICE_SYSTEM_OBJ): $(DEVICE_SYSTEM) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/$(DEVICE_STARTUP_OBJ): $(DEVICE_STARTUP) | $(BUILD_DIR)
	$(CC) $(ASFLAGS) -c $< -o $@

$(ELF): $(OBJS) linker.ld
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BIN): $(ELF) $(IMAGE_FINALIZER) $(REPO_ROOT)/config.mk
	$(PYTHON) $(IMAGE_FINALIZER) \
	  --elf $(ELF) \
	  --bin $@ \
	  --version $(APP_VERSION) \
	  --objcopy $(OBJCOPY)

$(TRACE_MAP): $(BIN) $(TRACE_EXTRACTOR)
	$(PYTHON) $(TRACE_EXTRACTOR) \
	  --elf $(ELF) \
	  --map $@ \
	  --image app \
	  --nm $(NM) \
	  --objcopy $(OBJCOPY)

size: $(ELF)
	$(SIZE) $<

flash: $(BIN) $(TRACE_MAP)
	$(ST_FLASH) --reset write $(BIN) $(APP_SLOT_A_FLASH_ADDR)

flash-swo:
	$(MAKE) -B flash \
	  BUILD_DIR=$(SWO_BUILD_DIR) \
	  DEFINES="$(DEFINES) -DENABLE_SWO_TRACE"

flash-fault-test:
	$(MAKE) -B flash \
	  BUILD_DIR=$(SWO_BUILD_DIR) \
	  DEFINES="$(DEFINES) -DENABLE_SWO_TRACE -DENABLE_FAULT_TEST"

flash-watchdog-test:
	$(MAKE) -B flash \
	  BUILD_DIR=$(SWO_BUILD_DIR) \
	  DEFINES="$(DEFINES) -DENABLE_SWO_TRACE -DENABLE_WATCHDOG_TEST"

probe:
	$(ST_INFO) --probe

reset:
	$(ST_FLASH) reset

erase:
	$(ST_FLASH) erase

debug-server:
	$(OPENOCD) -f interface/stlink.cfg -f target/$(OPENOCD_TARGET).cfg

gdb: $(ELF)
	$(GDB) $(ELF)

swo-trace:
	$(OPENOCD) -f interface/stlink.cfg -f target/$(OPENOCD_TARGET).cfg \
	  -c "init" \
	  -c "reset halt" \
	  -c "$(OPENOCD_TPIU) configure -protocol uart -traceclk $(SWO_CLOCK) -pin-freq $(SWO_BAUD) -formatter 0 -output $(SWO_LOG)" \
	  -c "$(OPENOCD_TPIU) enable" \
	  -c "resume"

clean:
	rm -rf $(BUILD_DIR)

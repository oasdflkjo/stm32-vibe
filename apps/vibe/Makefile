PROJECT := vibe
BUILD_DIR := build
TEST_DIR := build/test
APP_NAME ?= $(PROJECT)
APP_SLOT ?= A

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

ifeq ($(APP_SLOT),A)
APP_FLASH_ADDR := $(APP_SLOT_A_FLASH_ADDR)
else ifeq ($(APP_SLOT),B)
APP_FLASH_ADDR := $(APP_SLOT_B_FLASH_ADDR)
else
$(error Unknown APP_SLOT '$(APP_SLOT)'; expected A or B)
endif
APP_FLASH_SIZE := $(APP_SLOT_SIZE)

DEFINES   := $(BOARD_DEFINES) -DTRACE_SWO_BAUD=$(SWO_BAUD)U -DWATCHDOG_TIMEOUT_MS=$(WATCHDOG_TIMEOUT_MS)U -DBOOT_CONFIRM_STABILIZATION_MS=$(BOOT_CONFIRM_STABILIZATION_MS)U $(EXTRA_DEFINES)
INCLUDES  := \
  -Isrc \
  -I$(SHARED) \
  $(BOARD_INCLUDES)

CFLAGS  := $(MCU_FLAGS) $(DEFINES) $(INCLUDES) -std=c11 -Wall -Wextra -Werror -Os -g3 -ffunction-sections -fdata-sections -fpic -msingle-pic-base -mpic-register=r9 -mno-pic-data-is-text-relative
ASFLAGS := $(MCU_FLAGS) -g3
LDFLAGS := $(MCU_FLAGS) -Tlinker.ld -Wl,--defsym=APP_FLASH_LENGTH=$(APP_FLASH_SIZE) -Wl,--defsym=RAM_LENGTH=$(RAM_LENGTH) -Wl,--no-relax -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(PROJECT).map --specs=nano.specs --specs=nosys.specs

APP_LOCAL_SRCS := \
  src/app.c \
  src/app_manifest.c \
  src/led_task.c

HAL_SRCS := \
  $(SHARED)/platform/runtime.c \
  $(PIC_STARTUP_SRC) \
  $(SHARED)/platform/boot_confirmation.c \
  $(SHARED)/platform/boot_confirmation_store.c \
  $(SHARED)/platform/boot_info.c \
  $(SHARED)/platform/health_supervisor.c \
  $(SHARED)/platform/update_agent.c \
  $(SHARED)/boot/boot_state.c \
  $(SHARED)/boot/boot_runtime.c \
  $(SHARED)/fault/fault_report.c \
  $(FAULT_IMPL_SRC) \
  $(SHARED)/libc/syscalls.c \
  $(HAL_IMPL_DIR)/critical.c \
  $(HAL_IMPL_DIR)/boot_state_store.c \
  $(HAL_IMPL_DIR)/gpio.c \
  $(HAL_IMPL_DIR)/systick.c \
  $(HAL_IMPL_DIR)/itm.c \
  $(HAL_IMPL_DIR)/uart.c \
  $(HAL_IMPL_DIR)/watchdog.c \
  $(CAN_IMPL_SRC) \
  $(SHARED)/trace/trace.c \
  $(SHARED)/update/update_protocol.c \
  $(SHARED)/update/update_stream.c \
  $(SHARED)/update/update_can.c

LOCAL_OBJS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(APP_LOCAL_SRCS))
HAL_C_OBJS := $(patsubst $(SHARED)/%.c,$(BUILD_DIR)/shared/%.o,$(filter %.c,$(HAL_SRCS)))
HAL_S_OBJS := $(patsubst $(SHARED)/%.s,$(BUILD_DIR)/shared/%.o,$(filter %.s,$(HAL_SRCS)))
HAL_OBJS   := $(HAL_C_OBJS) $(HAL_S_OBJS)

OBJS := \
  $(LOCAL_OBJS) \
  $(HAL_OBJS) \
  $(BUILD_DIR)/$(DEVICE_SYSTEM_OBJ) \
  $(BUILD_DIR)/$(DEVICE_STARTUP_OBJ)

ELF := $(BUILD_DIR)/$(PROJECT).elf
BIN := $(BUILD_DIR)/$(PROJECT).bin
METADATA := $(BUILD_DIR)/$(PROJECT).json

.PHONY: firmware firmware-slot-b firmware-can-smoke test clean size flash flash-slot-b flash-swo flash-fault-test flash-watchdog-test probe reset erase debug-server gdb swo-trace

firmware: $(ELF) $(BIN) $(TRACE_MAP) size

firmware-slot-b:
	$(MAKE) firmware APP_SLOT=B BUILD_DIR=build/slot-b

firmware-can-smoke:
	$(MAKE) firmware BUILD_DIR=build/can-smoke EXTRA_DEFINES=-DENABLE_CAN_SMOKE_TEST

# ── Unit tests (host gcc, mock HAL) ──────────────────────────────────────────
UNITY := $(REPO_ROOT)/vendor/unity/src

TEST_LED_SRCS := \
  test/test_led_task.c \
  src/led_task.c \
  $(UNITY)/unity.c

TEST_APP_SRCS := \
  test/test_app.c \
  src/app.c \
  src/led_task.c \
  $(UNITY)/unity.c

TEST_HEALTH_SRCS := \
  test/test_health_supervisor.c \
  $(SHARED)/platform/health_supervisor.c \
  $(UNITY)/unity.c

TEST_BOOT_CONFIRM_SRCS := \
  test/test_boot_confirmation.c \
  $(SHARED)/platform/boot_confirmation.c \
  $(UNITY)/unity.c

TEST_BOOT_INFO_SRCS := \
  test/test_boot_info.c \
  $(SHARED)/platform/boot_info.c \
  $(SHARED)/boot/boot_runtime.c \
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

TEST_UPDATE_PROTOCOL_SRCS := \
  test/test_update_protocol.c \
  $(SHARED)/update/update_protocol.c \
  $(UNITY)/unity.c

TEST_UPDATE_STREAM_SRCS := \
  test/test_update_stream.c \
  $(SHARED)/update/update_protocol.c \
  $(SHARED)/update/update_stream.c \
  $(UNITY)/unity.c

TEST_UPDATE_AGENT_SRCS := \
  test/test_update_agent.c \
  $(SHARED)/platform/update_agent.c \
  $(SHARED)/hal_impl/mock/uart.c \
  $(SHARED)/update/update_protocol.c \
  $(SHARED)/update/update_stream.c \
  $(UNITY)/unity.c

TEST_UPDATE_CAN_SRCS := \
  test/test_update_can.c \
  $(SHARED)/update/update_can.c \
  $(UNITY)/unity.c

TEST_LED_BIN := $(TEST_DIR)/test_led_task
TEST_APP_BIN := $(TEST_DIR)/test_app
TEST_HEALTH_BIN := $(TEST_DIR)/test_health_supervisor
TEST_BOOT_CONFIRM_BIN := $(TEST_DIR)/test_boot_confirmation
TEST_BOOT_INFO_BIN := $(TEST_DIR)/test_boot_info
TEST_TRACE_BIN := $(TEST_DIR)/test_trace
TEST_FAULT_BIN := $(TEST_DIR)/test_fault_report
TEST_WATCHDOG_BIN := $(TEST_DIR)/test_watchdog
TEST_CAN_BIN := $(TEST_DIR)/test_can
TEST_UPDATE_PROTOCOL_BIN := $(TEST_DIR)/test_update_protocol
TEST_UPDATE_STREAM_BIN := $(TEST_DIR)/test_update_stream
TEST_UPDATE_AGENT_BIN := $(TEST_DIR)/test_update_agent
TEST_UPDATE_CAN_BIN := $(TEST_DIR)/test_update_can

TEST_CFLAGS := -Isrc -I$(SHARED) -I$(UNITY) -std=c11 -Wall -Wextra -Werror -g

test: $(TEST_LED_BIN) $(TEST_APP_BIN) $(TEST_HEALTH_BIN) $(TEST_BOOT_CONFIRM_BIN) $(TEST_BOOT_INFO_BIN) $(TEST_TRACE_BIN) $(TEST_FAULT_BIN) $(TEST_WATCHDOG_BIN) $(TEST_CAN_BIN) $(TEST_UPDATE_PROTOCOL_BIN) $(TEST_UPDATE_STREAM_BIN) $(TEST_UPDATE_AGENT_BIN) $(TEST_UPDATE_CAN_BIN)
	./$(TEST_LED_BIN)
	./$(TEST_APP_BIN)
	./$(TEST_HEALTH_BIN)
	./$(TEST_BOOT_CONFIRM_BIN)
	./$(TEST_BOOT_INFO_BIN)
	./$(TEST_TRACE_BIN)
	./$(TEST_FAULT_BIN)
	./$(TEST_WATCHDOG_BIN)
	./$(TEST_CAN_BIN)
	./$(TEST_UPDATE_PROTOCOL_BIN)
	./$(TEST_UPDATE_STREAM_BIN)
	./$(TEST_UPDATE_AGENT_BIN)
	./$(TEST_UPDATE_CAN_BIN)
	cd $(REPO_ROOT) && $(PYTHON) -m unittest tools.test_trace_tools tools.test_finalize_image tools.test_uart_update tools.test_boot_state_image tools.test_waveshare_usb_can tools.test_can_update

$(TEST_LED_BIN): $(TEST_LED_SRCS) | $(TEST_DIR)
	gcc $(TEST_CFLAGS) $^ -o $@

$(TEST_APP_BIN): $(TEST_APP_SRCS) | $(TEST_DIR)
	gcc $(TEST_CFLAGS) $^ -o $@

$(TEST_HEALTH_BIN): $(TEST_HEALTH_SRCS) | $(TEST_DIR)
	gcc $(TEST_CFLAGS) $^ -o $@

$(TEST_BOOT_CONFIRM_BIN): $(TEST_BOOT_CONFIRM_SRCS) | $(TEST_DIR)
	gcc $(TEST_CFLAGS) $^ -o $@

$(TEST_BOOT_INFO_BIN): $(TEST_BOOT_INFO_SRCS) | $(TEST_DIR)
	gcc $(TEST_CFLAGS) $^ -o $@

$(TEST_TRACE_BIN): $(TEST_TRACE_SRCS) | $(TEST_DIR)
	gcc $(TEST_CFLAGS) $^ -o $@

$(TEST_FAULT_BIN): $(TEST_FAULT_SRCS) | $(TEST_DIR)
	gcc $(TEST_CFLAGS) -DENABLE_SWO_TRACE $^ -o $@

$(TEST_WATCHDOG_BIN): $(TEST_WATCHDOG_SRCS) | $(TEST_DIR)
	gcc -Itest/mock_stm32 $(TEST_CFLAGS) $^ -o $@

$(TEST_CAN_BIN): $(TEST_CAN_SRCS) | $(TEST_DIR)
	gcc $(TEST_CFLAGS) $^ -o $@

$(TEST_UPDATE_PROTOCOL_BIN): $(TEST_UPDATE_PROTOCOL_SRCS) | $(TEST_DIR)
	gcc $(TEST_CFLAGS) $^ -o $@

$(TEST_UPDATE_STREAM_BIN): $(TEST_UPDATE_STREAM_SRCS) | $(TEST_DIR)
	gcc $(TEST_CFLAGS) $^ -o $@

$(TEST_UPDATE_AGENT_BIN): $(TEST_UPDATE_AGENT_SRCS) | $(TEST_DIR)
	gcc $(TEST_CFLAGS) $^ -o $@

$(TEST_UPDATE_CAN_BIN): $(TEST_UPDATE_CAN_SRCS) | $(TEST_DIR)
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

$(BUILD_DIR)/shared/%.o: $(SHARED)/%.s | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

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
	  --app-name $(APP_NAME) \
	  --board-name "$(BOARD_NAME)" \
	  --metadata $(METADATA) \
	  --objcopy $(OBJCOPY) \
	  --nm $(NM)

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
	$(ST_FLASH) --reset write $(BIN) $(APP_FLASH_ADDR)

flash-slot-b:
	$(MAKE) flash APP_SLOT=B BUILD_DIR=build/slot-b

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

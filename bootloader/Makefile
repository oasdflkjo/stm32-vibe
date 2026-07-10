PROJECT := bootloader
BUILD_DIR := build
TEST_DIR := build/test

REPO_ROOT := ..
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

DEFINES   := \
  $(BOARD_DEFINES) \
  -DENABLE_SWO_TRACE \
  -DTRACE_ID_BASE=0x8000U \
  -DTRACE_SWO_BAUD=$(SWO_BAUD)U
INCLUDES  := \
  -I$(SHARED) \
  $(BOARD_INCLUDES)

CFLAGS  := $(MCU_FLAGS) $(DEFINES) $(INCLUDES) -std=c11 -Wall -Wextra -Werror -Os -g3 -ffunction-sections -fdata-sections
ASFLAGS := $(MCU_FLAGS) -g3
LDFLAGS := $(MCU_FLAGS) -Tlinker.ld -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(PROJECT).map --specs=nano.specs --specs=nosys.specs

SRCS := \
  src/main.c \
  src/app_image.c \
  src/app_validation.c \
  src/boot_flash_stm32l1.c \
  src/boot_policy.c \
  src/update_session.c \
  src/update_command.c

SHARED_SRCS := \
  $(SHARED)/boot/boot_state.c \
  $(SHARED)/boot/boot_runtime.c \
  $(HAL_IMPL_DIR)/boot_state_store.c \
  $(SHARED)/fault/fault_report.c \
  $(FAULT_IMPL_SRC) \
  $(SHARED)/libc/syscalls.c \
  $(HAL_IMPL_DIR)/critical.c \
  $(HAL_IMPL_DIR)/itm.c \
  $(HAL_IMPL_DIR)/uart.c \
  $(SHARED)/trace/trace.c \
  $(SHARED)/update/update_protocol.c \
  $(SHARED)/update/update_stream.c

OBJS := \
  $(patsubst src/%,$(BUILD_DIR)/%,$(SRCS:.c=.o)) \
  $(patsubst $(SHARED)/%.c,$(BUILD_DIR)/shared/%.o,$(SHARED_SRCS)) \
  $(BUILD_DIR)/$(DEVICE_SYSTEM_OBJ) \
  $(BUILD_DIR)/$(DEVICE_STARTUP_OBJ)

ELF := $(BUILD_DIR)/$(PROJECT).elf
BIN := $(BUILD_DIR)/$(PROJECT).bin
TRACE_MAP := $(BUILD_DIR)/trace_map.json
TRACE_EXTRACTOR := $(REPO_ROOT)/tools/extract_trace_map.py

.PHONY: firmware test clean size flash probe reset erase debug-server gdb

firmware: $(ELF) $(BIN) $(TRACE_MAP) size

UNITY := $(REPO_ROOT)/vendor/unity/src
TEST_APP_VALIDATION_BIN := $(TEST_DIR)/test_app_validation
TEST_APP_VALIDATION_SRCS := \
  test/test_app_validation.c \
  src/app_image.c \
  src/app_validation.c \
  $(UNITY)/unity.c
TEST_BOOT_STATE_BIN := $(TEST_DIR)/test_boot_state
TEST_BOOT_STATE_SRCS := \
  test/test_boot_state.c \
  $(SHARED)/boot/boot_state.c \
  $(UNITY)/unity.c
TEST_BOOT_RUNTIME_BIN := $(TEST_DIR)/test_boot_runtime
TEST_BOOT_RUNTIME_SRCS := \
  test/test_boot_runtime.c \
  $(SHARED)/boot/boot_runtime.c \
  $(UNITY)/unity.c
TEST_BOOT_POLICY_BIN := $(TEST_DIR)/test_boot_policy
TEST_BOOT_POLICY_SRCS := \
  test/test_boot_policy.c \
  src/boot_policy.c \
  $(SHARED)/boot/boot_state.c \
  $(UNITY)/unity.c
TEST_UPDATE_COMMAND_BIN := $(TEST_DIR)/test_update_command
TEST_UPDATE_COMMAND_SRCS := \
  test/test_update_command.c \
  src/update_command.c \
  src/app_image.c \
  src/app_validation.c \
  src/boot_policy.c \
  src/update_session.c \
  test/boot_flash_mock.c \
  test/boot_state_store_mock.c \
  $(SHARED)/boot/boot_state.c \
  $(SHARED)/hal_impl/mock/uart.c \
  $(SHARED)/update/update_protocol.c \
  $(SHARED)/update/update_stream.c \
  $(UNITY)/unity.c
TEST_UPDATE_SESSION_BIN := $(TEST_DIR)/test_update_session
TEST_UPDATE_SESSION_SRCS := \
  test/test_update_session.c \
  src/update_session.c \
  src/app_image.c \
  src/app_validation.c \
  src/boot_policy.c \
  test/boot_flash_mock.c \
  test/boot_state_store_mock.c \
  $(SHARED)/boot/boot_state.c \
  $(UNITY)/unity.c
TEST_CFLAGS := -Isrc -I$(SHARED) -I$(UNITY) -std=c11 -Wall -Wextra -Werror -g

test: $(TEST_APP_VALIDATION_BIN) $(TEST_BOOT_STATE_BIN) $(TEST_BOOT_RUNTIME_BIN) $(TEST_BOOT_POLICY_BIN) $(TEST_UPDATE_COMMAND_BIN) $(TEST_UPDATE_SESSION_BIN)
	./$(TEST_APP_VALIDATION_BIN)
	./$(TEST_BOOT_STATE_BIN)
	./$(TEST_BOOT_RUNTIME_BIN)
	./$(TEST_BOOT_POLICY_BIN)
	./$(TEST_UPDATE_COMMAND_BIN)
	./$(TEST_UPDATE_SESSION_BIN)

$(TEST_APP_VALIDATION_BIN): $(TEST_APP_VALIDATION_SRCS) | $(TEST_DIR)
	gcc $(TEST_CFLAGS) $^ -o $@

$(TEST_BOOT_STATE_BIN): $(TEST_BOOT_STATE_SRCS) | $(TEST_DIR)
	gcc $(TEST_CFLAGS) $^ -o $@

$(TEST_BOOT_RUNTIME_BIN): $(TEST_BOOT_RUNTIME_SRCS) | $(TEST_DIR)
	gcc $(TEST_CFLAGS) $^ -o $@

$(TEST_BOOT_POLICY_BIN): $(TEST_BOOT_POLICY_SRCS) | $(TEST_DIR)
	gcc $(TEST_CFLAGS) $^ -o $@

$(TEST_UPDATE_COMMAND_BIN): $(TEST_UPDATE_COMMAND_SRCS) | $(TEST_DIR)
	gcc $(TEST_CFLAGS) $^ -o $@

$(TEST_UPDATE_SESSION_BIN): $(TEST_UPDATE_SESSION_SRCS) | $(TEST_DIR)
	gcc $(TEST_CFLAGS) $^ -o $@

$(TEST_DIR):
	mkdir -p $@

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

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

$(TRACE_MAP): $(ELF) $(TRACE_EXTRACTOR)
	$(PYTHON) $(TRACE_EXTRACTOR) \
	  --elf $(ELF) \
	  --map $@ \
	  --image bootloader \
	  --id-base 0x8000 \
	  --nm $(NM) \
	  --objcopy $(OBJCOPY)

size: $(ELF)
	$(SIZE) $<

flash: $(BIN) $(TRACE_MAP)
	$(ST_FLASH) --reset write $(BIN) $(BOOTLOADER_FLASH_ADDR)

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

clean:
	rm -rf $(BUILD_DIR)

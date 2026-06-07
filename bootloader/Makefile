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

MCU_FLAGS := -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
DEFINES   := \
  -DSTM32L152xE \
  -DENABLE_SWO_TRACE \
  -DTRACE_ID_BASE=0x8000U \
  -DTRACE_SWO_BAUD=$(SWO_BAUD)U
INCLUDES  := \
  -I$(SHARED) \
  -I$(VENDOR)/cmsis-core/CMSIS/Core/Include \
  -I$(VENDOR)/cmsis_device_l1/Include

CFLAGS  := $(MCU_FLAGS) $(DEFINES) $(INCLUDES) -std=c11 -Wall -Wextra -Werror -Os -g3 -ffunction-sections -fdata-sections
ASFLAGS := $(MCU_FLAGS) -g3
LDFLAGS := $(MCU_FLAGS) -Tlinker.ld -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(PROJECT).map --specs=nano.specs --specs=nosys.specs

SRCS := \
  src/main.c \
  src/app_validation.c

SHARED_SRCS := \
  $(SHARED)/fault/fault_report.c \
  $(SHARED)/fault/fault_stm32l1.c \
  $(SHARED)/libc/syscalls.c \
  $(SHARED)/hal_impl/stm32l1/critical.c \
  $(SHARED)/hal_impl/stm32l1/itm.c \
  $(SHARED)/trace/trace.c

DEVICE_SYSTEM  := $(VENDOR)/cmsis_device_l1/Source/Templates/system_stm32l1xx.c
DEVICE_STARTUP := $(VENDOR)/cmsis_device_l1/Source/Templates/gcc/startup_stm32l152xe.s

OBJS := \
  $(patsubst src/%,$(BUILD_DIR)/%,$(SRCS:.c=.o)) \
  $(patsubst $(SHARED)/%.c,$(BUILD_DIR)/shared/%.o,$(SHARED_SRCS)) \
  $(BUILD_DIR)/system_stm32l1xx.o \
  $(BUILD_DIR)/startup_stm32l152xe.o

ELF := $(BUILD_DIR)/$(PROJECT).elf
BIN := $(BUILD_DIR)/$(PROJECT).bin
TRACE_MAP := $(BUILD_DIR)/trace_map.json
TRACE_EXTRACTOR := $(REPO_ROOT)/tools/extract_trace_map.py

.PHONY: firmware test clean size flash probe reset erase debug-server gdb

firmware: $(ELF) $(BIN) $(TRACE_MAP) size

UNITY := $(REPO_ROOT)/vendor/unity/src
TEST_BIN := $(TEST_DIR)/test_app_validation
TEST_SRCS := \
  test/test_app_validation.c \
  src/app_validation.c \
  $(UNITY)/unity.c
TEST_CFLAGS := -Isrc -I$(UNITY) -std=c11 -Wall -Wextra -Werror -g

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRCS) | $(TEST_DIR)
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

$(BUILD_DIR)/system_stm32l1xx.o: $(DEVICE_SYSTEM) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/startup_stm32l152xe.o: $(DEVICE_STARTUP) | $(BUILD_DIR)
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

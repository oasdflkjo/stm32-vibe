CONTAINER_ENGINE ?= podman
CONTAINER_IMAGE ?= stm32-vibe-build
CONTAINER_VOLUME_SUFFIX ?= :Z

include config.mk

export CCACHE_DISABLE ?= 1

APP ?= vibe
APP_DIR := apps/$(APP)

COMBINED_DIR := build
COMBINED_HEX := $(COMBINED_DIR)/combined.hex
COMBINED_SLOTS_HEX := $(COMBINED_DIR)/combined-slots.hex
COMBINED_SLOTS_BIN := $(COMBINED_DIR)/combined-slots.bin

ALL_PROJECTS := bootloader $(APP_DIR)

.PHONY: all _firmware _combined clean test combined-slots flash-combined-slots flash flash-swo flash-fault-test flash-watchdog-test flash-bootloader flash-bootloader-only flash-app firmware-slot-b flash-app-slot-b container-build container-shell $(ALL_PROJECTS)

# ── Developer entry points (run everything inside the container) ──────────────
all: container-build
	$(CONTAINER_ENGINE) run --rm -v "$(CURDIR):/workspace$(CONTAINER_VOLUME_SUFFIX)" -w /workspace $(CONTAINER_IMAGE) make _combined

container-shell: container-build
	$(CONTAINER_ENGINE) run --rm -it -v "$(CURDIR):/workspace$(CONTAINER_VOLUME_SUFFIX)" -w /workspace $(CONTAINER_IMAGE) bash

container-build:
	$(CONTAINER_ENGINE) build -t $(CONTAINER_IMAGE) -f Containerfile .

# ── Flash targets (run on host, require st-flash + USB access) ────────────────
flash: $(COMBINED_HEX)
	st-flash --reset --format ihex write $(COMBINED_HEX)

combined-slots:
	tools/build_flash_image.sh \
		--output $(COMBINED_SLOTS_HEX) \
		--bin-output $(COMBINED_SLOTS_BIN) \
		--flash-addr $(BOOTLOADER_FLASH_ADDR) \
		--boot-state-addr $(BOOT_STATE_FLASH_ADDR) \
		--boot-state-size $(BOOT_STATE_FLASH_SIZE) \
		--slot-a-addr $(APP_SLOT_A_FLASH_ADDR) \
		--slot-b-addr $(APP_SLOT_B_FLASH_ADDR)

flash-combined-slots:
	tools/build_flash_image.sh \
		--output $(COMBINED_SLOTS_HEX) \
		--bin-output $(COMBINED_SLOTS_BIN) \
		--flash-addr $(BOOTLOADER_FLASH_ADDR) \
		--boot-state-addr $(BOOT_STATE_FLASH_ADDR) \
		--boot-state-size $(BOOT_STATE_FLASH_SIZE) \
		--slot-a-addr $(APP_SLOT_A_FLASH_ADDR) \
		--slot-b-addr $(APP_SLOT_B_FLASH_ADDR) \
		--flash

flash-bootloader: bootloader/build/bootloader.bin
	st-flash --reset write bootloader/build/bootloader.bin $(BOOTLOADER_FLASH_ADDR)

flash-bootloader-only: bootloader/build/bootloader.bin
	st-flash erase
	st-flash --reset write bootloader/build/bootloader.bin $(BOOTLOADER_FLASH_ADDR)

flash-app:
	$(MAKE) -C $(APP_DIR) flash

firmware-slot-b:
	$(MAKE) -C $(APP_DIR) firmware-slot-b

flash-app-slot-b:
	$(MAKE) -C $(APP_DIR) flash-slot-b

flash-swo:
	$(MAKE) -C bootloader flash
	$(MAKE) -C $(APP_DIR) flash-swo

flash-fault-test:
	$(MAKE) -C bootloader flash
	$(MAKE) -C $(APP_DIR) flash-fault-test

flash-watchdog-test:
	$(MAKE) -C bootloader flash
	$(MAKE) -C $(APP_DIR) flash-watchdog-test

clean:
	for proj in $(ALL_PROJECTS); do $(MAKE) -C $$proj clean; done
	rm -rf $(COMBINED_DIR)

test:
	$(MAKE) -C bootloader test
	$(MAKE) -C $(APP_DIR) test

# ── Internal targets (used inside the container / CI only) ────────────────────
_firmware: $(ALL_PROJECTS)

$(ALL_PROJECTS):
	$(MAKE) -C $@ firmware

_combined: _firmware
	mkdir -p $(COMBINED_DIR)
	arm-none-eabi-objcopy -O ihex bootloader/build/bootloader.elf $(COMBINED_DIR)/bootloader.hex
	arm-none-eabi-objcopy -O ihex $(APP_DIR)/build/$(APP).elf $(COMBINED_DIR)/$(APP).hex
	grep -v ':00000001FF' $(COMBINED_DIR)/bootloader.hex > $(COMBINED_HEX)
	cat $(COMBINED_DIR)/$(APP).hex >> $(COMBINED_HEX)
	@echo "Combined image: $(COMBINED_HEX)"

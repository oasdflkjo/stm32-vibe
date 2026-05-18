CONTAINER_ENGINE ?= podman
CONTAINER_IMAGE ?= stm32-vibe-build
CONTAINER_VOLUME_SUFFIX ?= :Z

export CCACHE_DISABLE ?= 1

COMBINED_DIR := build
COMBINED_HEX := $(COMBINED_DIR)/combined.hex

APPS := $(wildcard apps/*)
ALL_PROJECTS := bootloader $(APPS)

.PHONY: all _firmware _combined clean test flash flash-bootloader flash-app container-build container-shell $(ALL_PROJECTS)

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

flash-bootloader: bootloader/build/bootloader.bin
	st-flash --reset write bootloader/build/bootloader.bin 0x08000000

flash-app: apps/vibe/build/vibe.bin
	st-flash --reset write apps/vibe/build/vibe.bin 0x08004000

clean:
	for proj in $(ALL_PROJECTS); do $(MAKE) -C $$proj clean; done
	rm -rf $(COMBINED_DIR)

test:
	$(MAKE) -C apps/vibe test

# ── Internal targets (used inside the container / CI only) ────────────────────
_firmware: $(ALL_PROJECTS)

$(ALL_PROJECTS):
	$(MAKE) -C $@ firmware

_combined: _firmware
	mkdir -p $(COMBINED_DIR)
	arm-none-eabi-objcopy -O ihex bootloader/build/bootloader.elf $(COMBINED_DIR)/bootloader.hex
	arm-none-eabi-objcopy -O ihex apps/vibe/build/vibe.elf $(COMBINED_DIR)/vibe.hex
	grep -v ':00000001FF' $(COMBINED_DIR)/bootloader.hex > $(COMBINED_HEX)
	cat $(COMBINED_DIR)/vibe.hex >> $(COMBINED_HEX)
	@echo "Combined image: $(COMBINED_HEX)"

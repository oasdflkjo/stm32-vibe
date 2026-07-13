CONFIG_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
REPO_ROOT ?= .

BOARD ?= nucleo-l152re
BOARD_CONFIG := $(CONFIG_DIR)boards/$(BOARD).mk

ifeq ($(wildcard $(BOARD_CONFIG)),)
$(error Unknown BOARD '$(BOARD)'; expected a file at $(BOARD_CONFIG))
endif

include $(BOARD_CONFIG)

APP_VERSION := 1
WATCHDOG_TIMEOUT_MS := 4000
BOOT_CONFIRM_STABILIZATION_MS := 5000

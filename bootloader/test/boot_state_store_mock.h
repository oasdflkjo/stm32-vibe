#pragma once

#include "boot/boot_state_store.h"

void boot_state_store_mock_reset(void);
void boot_state_store_mock_set_state(const boot_state_record_t *state);
void boot_state_store_mock_set_save_allowed(int allowed);
const boot_state_record_t *boot_state_store_mock_state(void);

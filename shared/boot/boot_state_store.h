#pragma once

#include "boot/boot_state.h"

void boot_state_store_load(boot_state_record_t *state);
int boot_state_store_save_next(const boot_state_record_t *state);

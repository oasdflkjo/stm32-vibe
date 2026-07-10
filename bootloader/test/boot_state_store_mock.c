#include "boot/boot_state_store.h"

static boot_state_record_t stored_state;
static int has_state;
static int save_allowed = 1;

void boot_state_store_mock_reset(void)
{
    boot_state_init_default(&stored_state);
    has_state = 0;
    save_allowed = 1;
}

void boot_state_store_mock_set_state(const boot_state_record_t *state)
{
    stored_state = *state;
    has_state = 1;
}

void boot_state_store_mock_set_save_allowed(int allowed)
{
    save_allowed = allowed;
}

const boot_state_record_t *boot_state_store_mock_state(void)
{
    return has_state ? &stored_state : 0;
}

void boot_state_store_load(boot_state_record_t *state)
{
    if (has_state && (boot_state_validate(&stored_state) == BOOT_STATE_VALID)) {
        *state = stored_state;
        return;
    }

    boot_state_init_default(state);
}

int boot_state_store_save_next(const boot_state_record_t *state)
{
    if (!save_allowed) {
        return 0;
    }

    stored_state = *state;
    has_state = 1;
    return 1;
}

#include "platform/boot_confirmation.h"

#include "boot/boot_state.h"
#include "boot/boot_state_store.h"

bool platform_confirm_pending_boot(void)
{
    boot_state_record_t state;

    boot_state_store_load(&state);
    if (!boot_state_confirm_pending(&state)) {
        return true;
    }
    return boot_state_store_save_next(&state) != 0;
}

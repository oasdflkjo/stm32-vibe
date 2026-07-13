#include "platform/boot_info.h"

#include "boot/boot_runtime.h"

static platform_boot_info_t boot_info;
static bool boot_info_valid;

bool platform_boot_info_init(void)
{
    const boot_handoff_t *handoff =
        (const boot_handoff_t *)boot_handoff_shared();

    return platform_boot_info_init_from(handoff);
}

bool platform_boot_info_init_from(const boot_handoff_t *handoff)
{

    boot_info_valid = false;
    if (!boot_handoff_is_valid(handoff)) {
        return false;
    }
    boot_info = (platform_boot_info_t){
        .image_base = handoff->image_base,
        .image_size = handoff->image_size,
        .slot = handoff->slot,
        .boot_attempt = handoff->boot_attempt,
        .pending = handoff->state == BOOT_HANDOFF_PENDING,
    };
    boot_info_valid = true;
    return true;
}

bool platform_boot_info_get(platform_boot_info_t *info)
{
    if ((info == 0) || !boot_info_valid) {
        return false;
    }
    *info = boot_info;
    return true;
}

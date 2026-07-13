#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef bool (*boot_confirmation_commit_fn_t)(void);

typedef struct {
    boot_confirmation_commit_fn_t commit;
    uint32_t healthy_since_ms;
    uint32_t stabilization_ms;
    bool healthy_period_active;
    bool completed;
} boot_confirmation_t;

void boot_confirmation_init(boot_confirmation_t *confirmation,
                            uint32_t stabilization_ms,
                            boot_confirmation_commit_fn_t commit);
void boot_confirmation_poll(boot_confirmation_t *confirmation,
                            uint32_t now_ms,
                            bool healthy);
bool boot_confirmation_is_complete(const boot_confirmation_t *confirmation);
bool platform_confirm_pending_boot(void);

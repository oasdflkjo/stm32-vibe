#include "platform/boot_confirmation.h"

void boot_confirmation_init(boot_confirmation_t *confirmation,
                            uint32_t stabilization_ms,
                            boot_confirmation_commit_fn_t commit)
{
    if (confirmation != 0) {
        *confirmation = (boot_confirmation_t){
            .commit = commit,
            .stabilization_ms = stabilization_ms,
        };
    }
}

void boot_confirmation_poll(boot_confirmation_t *confirmation,
                            uint32_t now_ms,
                            bool healthy)
{
    if ((confirmation == 0) || confirmation->completed ||
        (confirmation->commit == 0)) {
        return;
    }
    if (!healthy) {
        confirmation->healthy_period_active = false;
        return;
    }
    if (!confirmation->healthy_period_active) {
        confirmation->healthy_since_ms = now_ms;
        confirmation->healthy_period_active = true;
        return;
    }
    if ((uint32_t)(now_ms - confirmation->healthy_since_ms) >=
        confirmation->stabilization_ms) {
        confirmation->completed = confirmation->commit();
    }
}

bool boot_confirmation_is_complete(const boot_confirmation_t *confirmation)
{
    return (confirmation != 0) && confirmation->completed;
}

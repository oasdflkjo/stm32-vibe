#include "platform/health_supervisor.h"

void health_supervisor_init(health_supervisor_t *supervisor,
                            uint32_t max_progress_age_ms)
{
    if (supervisor != 0) {
        *supervisor = (health_supervisor_t){
            .max_progress_age_ms = max_progress_age_ms,
        };
    }
}

void health_supervisor_report_application(health_supervisor_t *supervisor,
                                          bool healthy)
{
    if (supervisor != 0) {
        supervisor->application_healthy = healthy;
    }
}

void health_supervisor_report_progress(health_supervisor_t *supervisor,
                                       uint32_t now_ms)
{
    if (supervisor != 0) {
        supervisor->last_progress_ms = now_ms;
        supervisor->progress_seen = true;
    }
}

bool health_supervisor_may_feed_watchdog(const health_supervisor_t *supervisor,
                                         uint32_t now_ms)
{
    if ((supervisor == 0) || !supervisor->application_healthy ||
        !supervisor->progress_seen || (supervisor->max_progress_age_ms == 0U)) {
        return false;
    }

    return (uint32_t)(now_ms - supervisor->last_progress_ms) <=
           supervisor->max_progress_age_ms;
}

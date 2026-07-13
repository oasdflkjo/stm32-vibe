#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t last_progress_ms;
    uint32_t max_progress_age_ms;
    bool application_healthy;
    bool progress_seen;
} health_supervisor_t;

void health_supervisor_init(health_supervisor_t *supervisor,
                            uint32_t max_progress_age_ms);
void health_supervisor_report_application(health_supervisor_t *supervisor,
                                          bool healthy);
void health_supervisor_report_progress(health_supervisor_t *supervisor,
                                       uint32_t now_ms);
bool health_supervisor_may_feed_watchdog(const health_supervisor_t *supervisor,
                                         uint32_t now_ms);

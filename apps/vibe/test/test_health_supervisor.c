#include "platform/health_supervisor.h"
#include "unity.h"

static health_supervisor_t supervisor;

void setUp(void)
{
    health_supervisor_init(&supervisor, 100U);
}

void tearDown(void) {}

void test_supervisor_requires_health_and_progress(void)
{
    TEST_ASSERT_FALSE(health_supervisor_may_feed_watchdog(&supervisor, 0U));
    health_supervisor_report_application(&supervisor, true);
    TEST_ASSERT_FALSE(health_supervisor_may_feed_watchdog(&supervisor, 0U));
    health_supervisor_report_progress(&supervisor, 10U);
    TEST_ASSERT_TRUE(health_supervisor_may_feed_watchdog(&supervisor, 10U));
}

void test_unhealthy_application_prevents_watchdog_feed(void)
{
    health_supervisor_report_application(&supervisor, true);
    health_supervisor_report_progress(&supervisor, 10U);
    health_supervisor_report_application(&supervisor, false);
    TEST_ASSERT_FALSE(health_supervisor_may_feed_watchdog(&supervisor, 10U));
}

void test_stale_progress_prevents_watchdog_feed(void)
{
    health_supervisor_report_application(&supervisor, true);
    health_supervisor_report_progress(&supervisor, 10U);
    TEST_ASSERT_TRUE(health_supervisor_may_feed_watchdog(&supervisor, 110U));
    TEST_ASSERT_FALSE(health_supervisor_may_feed_watchdog(&supervisor, 111U));
}

void test_progress_age_handles_clock_wraparound(void)
{
    health_supervisor_report_application(&supervisor, true);
    health_supervisor_report_progress(&supervisor, UINT32_MAX - 20U);
    TEST_ASSERT_TRUE(health_supervisor_may_feed_watchdog(&supervisor, 30U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_supervisor_requires_health_and_progress);
    RUN_TEST(test_unhealthy_application_prevents_watchdog_feed);
    RUN_TEST(test_stale_progress_prevents_watchdog_feed);
    RUN_TEST(test_progress_age_handles_clock_wraparound);
    return UNITY_END();
}

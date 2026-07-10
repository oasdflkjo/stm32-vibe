#include "platform/boot_confirmation.h"
#include "unity.h"

static boot_confirmation_t confirmation;
static uint32_t commit_calls;
static bool commit_result;

static bool commit(void)
{
    commit_calls++;
    return commit_result;
}

void setUp(void)
{
    commit_calls = 0U;
    commit_result = true;
    boot_confirmation_init(&confirmation, 5000U, commit);
}

void tearDown(void) {}

void test_confirmation_requires_continuous_health_interval(void)
{
    boot_confirmation_poll(&confirmation, 100U, true);
    boot_confirmation_poll(&confirmation, 5099U, true);
    TEST_ASSERT_EQUAL_UINT32(0U, commit_calls);
    boot_confirmation_poll(&confirmation, 5100U, true);
    TEST_ASSERT_EQUAL_UINT32(1U, commit_calls);
    TEST_ASSERT_TRUE(boot_confirmation_is_complete(&confirmation));
}

void test_unhealthy_report_restarts_stabilization_interval(void)
{
    boot_confirmation_poll(&confirmation, 100U, true);
    boot_confirmation_poll(&confirmation, 4000U, false);
    boot_confirmation_poll(&confirmation, 5000U, true);
    boot_confirmation_poll(&confirmation, 9999U, true);
    TEST_ASSERT_EQUAL_UINT32(0U, commit_calls);
    boot_confirmation_poll(&confirmation, 10000U, true);
    TEST_ASSERT_EQUAL_UINT32(1U, commit_calls);
}

void test_successful_confirmation_is_one_shot(void)
{
    boot_confirmation_poll(&confirmation, 0U, true);
    boot_confirmation_poll(&confirmation, 5000U, true);
    boot_confirmation_poll(&confirmation, 6000U, true);
    TEST_ASSERT_EQUAL_UINT32(1U, commit_calls);
}

void test_failed_commit_is_retried_while_healthy(void)
{
    commit_result = false;
    boot_confirmation_poll(&confirmation, 0U, true);
    boot_confirmation_poll(&confirmation, 5000U, true);
    TEST_ASSERT_EQUAL_UINT32(1U, commit_calls);
    TEST_ASSERT_FALSE(boot_confirmation_is_complete(&confirmation));
    commit_result = true;
    boot_confirmation_poll(&confirmation, 5001U, true);
    TEST_ASSERT_EQUAL_UINT32(2U, commit_calls);
    TEST_ASSERT_TRUE(boot_confirmation_is_complete(&confirmation));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_confirmation_requires_continuous_health_interval);
    RUN_TEST(test_unhealthy_report_restarts_stabilization_interval);
    RUN_TEST(test_successful_confirmation_is_one_shot);
    RUN_TEST(test_failed_commit_is_retried_while_healthy);
    return UNITY_END();
}

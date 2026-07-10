#include "platform/application.h"
#include "unity.h"

static bool led_enabled;
static uint32_t led_writes;
static uint32_t reset_requests;

static void set_led(bool enabled)
{
    led_enabled = enabled;
    led_writes++;
}

static void request_reset(void)
{
    reset_requests++;
}

void setUp(void)
{
    led_enabled = true;
    led_writes = 0U;
    reset_requests = 0U;
}

void tearDown(void) {}

void test_app_rejects_missing_services(void)
{
    TEST_ASSERT_EQUAL(APP_INIT_FAILED, app_init(0));
}

void test_app_initializes_using_platform_services(void)
{
    const app_services_t services = {
        .status_led_set = set_led,
        .request_update_reset = request_reset,
    };

    TEST_ASSERT_EQUAL(APP_INIT_OK, app_init(&services));
    TEST_ASSERT_TRUE(app_is_healthy());
    TEST_ASSERT_FALSE(led_enabled);
    TEST_ASSERT_EQUAL_UINT32(1U, led_writes);
}

void test_app_processes_deadlines_without_hardware(void)
{
    const app_services_t services = {
        .status_led_set = set_led,
        .request_update_reset = request_reset,
    };

    TEST_ASSERT_EQUAL(APP_INIT_OK, app_init(&services));
    app_process(499U);
    TEST_ASSERT_FALSE(led_enabled);
    app_process(500U);
    TEST_ASSERT_TRUE(led_enabled);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_app_rejects_missing_services);
    RUN_TEST(test_app_initializes_using_platform_services);
    RUN_TEST(test_app_processes_deadlines_without_hardware);
    return UNITY_END();
}

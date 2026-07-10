#include "led_task.h"
#include "unity.h"

static bool output;
static uint32_t writes;

static void set_output(bool enabled)
{
    output = enabled;
    writes++;
}

void setUp(void)
{
    output = true;
    writes = 0U;
    led_task_init(set_output, 100U);
}

void tearDown(void) {}

void test_led_starts_off(void)
{
    TEST_ASSERT_FALSE(output);
    TEST_ASSERT_EQUAL_UINT32(1U, writes);
}

void test_led_toggles_only_at_deadline(void)
{
    led_task_process(599U);
    TEST_ASSERT_FALSE(output);
    led_task_process(600U);
    TEST_ASSERT_TRUE(output);
    led_task_process(1100U);
    TEST_ASSERT_FALSE(output);
}

void test_led_deadline_handles_clock_wraparound(void)
{
    led_task_init(set_output, UINT32_MAX - 100U);
    led_task_process(398U);
    TEST_ASSERT_FALSE(output);
    led_task_process(399U);
    TEST_ASSERT_TRUE(output);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_led_starts_off);
    RUN_TEST(test_led_toggles_only_at_deadline);
    RUN_TEST(test_led_deadline_handles_clock_wraparound);
    return UNITY_END();
}

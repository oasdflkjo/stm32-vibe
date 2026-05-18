#include "unity.h"
#include "led_task.h"
#include "hal/gpio.h"

void setUp(void)    { led_task_init(); }
void tearDown(void) {}

void test_led_starts_off(void)
{
    TEST_ASSERT_EQUAL(GPIO_PIN_RESET, gpio_led_get_state());
}

void test_led_toggles(void)
{
    led_task_run();
    TEST_ASSERT_EQUAL(GPIO_PIN_SET, gpio_led_get_state());
    led_task_run();
    TEST_ASSERT_EQUAL(GPIO_PIN_RESET, gpio_led_get_state());
}

void test_led_even_toggles_return_to_reset(void)
{
    for (int i = 0; i < 10; i++) {
        led_task_run();
    }
    /* 10 toggles from RESET → even → back to RESET */
    TEST_ASSERT_EQUAL(GPIO_PIN_RESET, gpio_led_get_state());
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_led_starts_off);
    RUN_TEST(test_led_toggles);
    RUN_TEST(test_led_even_toggles_return_to_reset);
    return UNITY_END();
}

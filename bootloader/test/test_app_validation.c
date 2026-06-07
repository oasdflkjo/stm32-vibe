#include "app_validation.h"
#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_accepts_valid_application_vectors(void)
{
    TEST_ASSERT_TRUE(
        app_vectors_are_valid(RAM_END_ADDR, APP_START_ADDR + 0x101U));
}

void test_rejects_stack_outside_ram(void)
{
    TEST_ASSERT_FALSE(
        app_vectors_are_valid(RAM_END_ADDR + 8U, APP_START_ADDR + 0x101U));
}

void test_rejects_misaligned_stack(void)
{
    TEST_ASSERT_FALSE(
        app_vectors_are_valid(RAM_END_ADDR - 4U, APP_START_ADDR + 0x101U));
}

void test_rejects_reset_handler_outside_app(void)
{
    TEST_ASSERT_FALSE(
        app_vectors_are_valid(RAM_END_ADDR, APP_START_ADDR - 1U));
}

void test_rejects_reset_handler_without_thumb_bit(void)
{
    TEST_ASSERT_FALSE(
        app_vectors_are_valid(RAM_END_ADDR, APP_START_ADDR + 0x100U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_accepts_valid_application_vectors);
    RUN_TEST(test_rejects_stack_outside_ram);
    RUN_TEST(test_rejects_misaligned_stack);
    RUN_TEST(test_rejects_reset_handler_outside_app);
    RUN_TEST(test_rejects_reset_handler_without_thumb_bit);
    return UNITY_END();
}

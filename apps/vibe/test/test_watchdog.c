#include "hal/watchdog.h"
#include "stm32l1xx.h"
#include "unity.h"
#include <string.h>

IWDG_TypeDef mock_iwdg;
DBGMCU_TypeDef mock_dbgmcu;

void setUp(void)
{
    memset(&mock_iwdg, 0, sizeof(mock_iwdg));
    memset(&mock_dbgmcu, 0, sizeof(mock_dbgmcu));
}

void tearDown(void)
{
}

void test_configures_four_second_nominal_timeout(void)
{
    TEST_ASSERT_TRUE(watchdog_init(4000U));
    TEST_ASSERT_EQUAL_HEX32(IWDG_PR_PR_2, mock_iwdg.PR);
    TEST_ASSERT_EQUAL_UINT32(2312U, mock_iwdg.RLR);
    TEST_ASSERT_EQUAL_HEX32(0xAAAAU, mock_iwdg.KR);
    TEST_ASSERT_BITS_HIGH(
        DBGMCU_APB1_FZ_DBG_IWDG_STOP,
        mock_dbgmcu.APB1FZ);
}

void test_rejects_zero_timeout_without_starting_watchdog(void)
{
    TEST_ASSERT_FALSE(watchdog_init(0U));
    TEST_ASSERT_EQUAL_HEX32(0U, mock_iwdg.KR);
}

void test_rejects_timeout_beyond_hardware_range(void)
{
    TEST_ASSERT_FALSE(watchdog_init(8000U));
    TEST_ASSERT_EQUAL_HEX32(0U, mock_iwdg.KR);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_configures_four_second_nominal_timeout);
    RUN_TEST(test_rejects_zero_timeout_without_starting_watchdog);
    RUN_TEST(test_rejects_timeout_beyond_hardware_range);
    return UNITY_END();
}

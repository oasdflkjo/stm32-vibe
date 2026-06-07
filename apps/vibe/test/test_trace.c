#include "hal/itm_mock.h"
#include "trace/trace.h"
#include "unity.h"

void setUp(void)
{
    itm_mock_reset();
}

void tearDown(void)
{
}

void test_trace_encodes_id_argument_and_crc(void)
{
    static const uint8_t expected[] = {
        0xA5U, 0x5AU,
        0x01U,
        0x34U, 0x12U,
        0x01U,
        0xEFU, 0xCDU, 0xABU, 0x89U,
        0x29U,
    };

    trace_emit1(0x1234U, 0x89ABCDEFU);

    TEST_ASSERT_EQUAL_UINT32(sizeof(expected), itm_mock_size());
    for (size_t i = 0U; i < sizeof(expected); i++) {
        TEST_ASSERT_EQUAL_HEX8(expected[i], itm_mock_byte(i));
    }
}

void test_trace_zero_argument_frame(void)
{
    trace_emit0(0x0001U);

    TEST_ASSERT_EQUAL_UINT32(7U, itm_mock_size());
    TEST_ASSERT_EQUAL_HEX8(0xA5U, itm_mock_byte(0U));
    TEST_ASSERT_EQUAL_HEX8(0x5AU, itm_mock_byte(1U));
    TEST_ASSERT_EQUAL_HEX8(0x01U, itm_mock_byte(2U));
    TEST_ASSERT_EQUAL_HEX8(0x01U, itm_mock_byte(3U));
    TEST_ASSERT_EQUAL_HEX8(0x00U, itm_mock_byte(4U));
    TEST_ASSERT_EQUAL_HEX8(0x00U, itm_mock_byte(5U));
    TEST_ASSERT_EQUAL_HEX8(0x7DU, itm_mock_byte(6U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_trace_encodes_id_argument_and_crc);
    RUN_TEST(test_trace_zero_argument_frame);
    return UNITY_END();
}

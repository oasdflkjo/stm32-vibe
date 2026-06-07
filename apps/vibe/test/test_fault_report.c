#include "fault/fault.h"
#include "hal/itm_mock.h"
#include "unity.h"

#define FRAME_HEADER_SIZE 6U

static uint32_t read_u32(size_t index)
{
    return (uint32_t)itm_mock_byte(index) |
           ((uint32_t)itm_mock_byte(index + 1U) << 8U) |
           ((uint32_t)itm_mock_byte(index + 2U) << 16U) |
           ((uint32_t)itm_mock_byte(index + 3U) << 24U);
}

void setUp(void)
{
    itm_mock_reset();
}

void tearDown(void)
{
}

void test_fault_report_emits_register_records(void)
{
    const fault_report_t report = {
        .type = FAULT_TYPE_BUS,
        .pc = 0x08004567U,
        .lr = 0x08001235U,
        .xpsr = 0x21000000U,
        .cfsr = 0x00008200U,
        .hfsr = 0x40000000U,
        .mmfar = 0x20000010U,
        .bfar = 0x40000020U,
    };
    const size_t first_frame = 0U;
    const size_t second_frame = 19U;
    const size_t third_frame = 34U;

    fault_report_emit(&report);

    TEST_ASSERT_EQUAL_UINT32(53U, itm_mock_size());

    TEST_ASSERT_EQUAL_UINT8(3U, itm_mock_byte(first_frame + 5U));
    TEST_ASSERT_EQUAL_UINT32(FAULT_TYPE_BUS,
                             read_u32(first_frame + FRAME_HEADER_SIZE));
    TEST_ASSERT_EQUAL_HEX32(report.pc,
                            read_u32(first_frame + FRAME_HEADER_SIZE + 4U));
    TEST_ASSERT_EQUAL_HEX32(report.lr,
                            read_u32(first_frame + FRAME_HEADER_SIZE + 8U));

    TEST_ASSERT_EQUAL_UINT8(2U, itm_mock_byte(second_frame + 5U));
    TEST_ASSERT_EQUAL_HEX32(report.cfsr,
                            read_u32(second_frame + FRAME_HEADER_SIZE));
    TEST_ASSERT_EQUAL_HEX32(report.hfsr,
                            read_u32(second_frame + FRAME_HEADER_SIZE + 4U));

    TEST_ASSERT_EQUAL_UINT8(3U, itm_mock_byte(third_frame + 5U));
    TEST_ASSERT_EQUAL_HEX32(report.mmfar,
                            read_u32(third_frame + FRAME_HEADER_SIZE));
    TEST_ASSERT_EQUAL_HEX32(report.bfar,
                            read_u32(third_frame + FRAME_HEADER_SIZE + 4U));
    TEST_ASSERT_EQUAL_HEX32(report.xpsr,
                            read_u32(third_frame + FRAME_HEADER_SIZE + 8U));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fault_report_emits_register_records);
    return UNITY_END();
}

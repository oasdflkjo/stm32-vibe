#include "hal/can_mock.h"
#include "unity.h"

void setUp(void)
{
    can_mock_reset();
}

void tearDown(void)
{
}

void test_can_requires_init_before_send(void)
{
    can_frame_t frame = {
        .id = 0x123U,
        .dlc = 1U,
        .data = {0x42U},
    };

    TEST_ASSERT_EQUAL(CAN_RESULT_NOT_READY, can_send(&frame));
}

void test_can_sends_frame_to_mock_tx_queue(void)
{
    can_frame_t sent = {
        .id = 0x123U,
        .dlc = 2U,
        .data = {0xCAU, 0xFEU},
    };
    can_frame_t received = {0};

    TEST_ASSERT_EQUAL(CAN_RESULT_OK, can_init(500000U));
    TEST_ASSERT_EQUAL(CAN_RESULT_OK, can_send(&sent));
    TEST_ASSERT_EQUAL(CAN_RESULT_OK, can_mock_pop_tx(&received));
    TEST_ASSERT_EQUAL_UINT32(sent.id, received.id);
    TEST_ASSERT_EQUAL_UINT8(sent.dlc, received.dlc);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(sent.data, received.data, sent.dlc);
    TEST_ASSERT_EQUAL_UINT32(1U, can_get_status().tx_count);
}

void test_can_receives_frame_from_mock_rx_queue(void)
{
    can_frame_t queued = {
        .id = 0x321U,
        .dlc = 3U,
        .data = {1U, 2U, 3U},
    };
    can_frame_t received = {0};

    TEST_ASSERT_EQUAL(CAN_RESULT_OK, can_init(500000U));
    TEST_ASSERT_EQUAL(CAN_RESULT_OK, can_mock_push_rx(&queued));
    TEST_ASSERT_EQUAL(CAN_RESULT_OK, can_receive(&received));
    TEST_ASSERT_EQUAL_UINT32(queued.id, received.id);
    TEST_ASSERT_EQUAL_UINT8(queued.dlc, received.dlc);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(queued.data, received.data, queued.dlc);
    TEST_ASSERT_EQUAL_UINT32(1U, can_get_status().rx_count);
}

void test_can_rejects_invalid_dlc(void)
{
    can_frame_t frame = {
        .id = 0x123U,
        .dlc = CAN_MAX_DATA_LEN + 1U,
    };

    TEST_ASSERT_EQUAL(CAN_RESULT_OK, can_init(500000U));
    TEST_ASSERT_EQUAL(CAN_RESULT_INVALID_ARGUMENT, can_send(&frame));
}

void test_can_reports_bus_off(void)
{
    can_frame_t frame = {
        .id = 0x123U,
        .dlc = 0U,
    };

    TEST_ASSERT_EQUAL(CAN_RESULT_OK, can_init(500000U));
    can_mock_set_bus_off(1);

    TEST_ASSERT_EQUAL(CAN_RESULT_BUS_OFF, can_send(&frame));
    TEST_ASSERT_EQUAL_UINT8(1U, can_get_status().bus_off);
    TEST_ASSERT_EQUAL_UINT32(1U, can_get_status().error_count);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_can_requires_init_before_send);
    RUN_TEST(test_can_sends_frame_to_mock_tx_queue);
    RUN_TEST(test_can_receives_frame_from_mock_rx_queue);
    RUN_TEST(test_can_rejects_invalid_dlc);
    RUN_TEST(test_can_reports_bus_off);
    return UNITY_END();
}

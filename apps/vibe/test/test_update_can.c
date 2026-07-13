#include "update/update_can.h"
#include "unity.h"

static uint8_t packet[UPDATE_PROTOCOL_MAX_PACKET_SIZE];

void setUp(void)
{
    for (size_t index = 0U; index < sizeof(packet); index++) {
        packet[index] = (uint8_t)index;
    }
}

void tearDown(void)
{
}

void test_fragments_and_reassembles_maximum_packet(void)
{
    update_can_fragmenter_t fragmenter;
    update_can_reassembly_t reassembly;
    can_frame_t frame;
    const uint8_t *decoded = 0;
    size_t decoded_len = 0U;
    uint32_t frame_count = 0U;

    TEST_ASSERT_TRUE(update_can_fragmenter_init(
        &fragmenter, update_can_request_id(1U), packet, sizeof(packet)));
    update_can_reassembly_init(&reassembly);
    while (update_can_fragmenter_next(&fragmenter, &frame)) {
        update_can_reassembly_result_t result = update_can_reassembly_feed(
            &reassembly, &frame, update_can_request_id(1U), &decoded,
            &decoded_len);
        frame_count++;
        if (fragmenter.offset < fragmenter.packet_len) {
            TEST_ASSERT_EQUAL(UPDATE_CAN_REASSEMBLY_NONE, result);
        } else {
            TEST_ASSERT_EQUAL(UPDATE_CAN_REASSEMBLY_COMPLETE, result);
        }
    }
    TEST_ASSERT_EQUAL_UINT32(22U, frame_count);
    TEST_ASSERT_EQUAL_UINT32(sizeof(packet), decoded_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(packet, decoded, sizeof(packet));
}

void test_final_fragment_is_zero_padded_to_dlc_eight(void)
{
    update_can_fragmenter_t fragmenter;
    can_frame_t frame;
    uint8_t short_packet[20] = {0};

    TEST_ASSERT_TRUE(update_can_fragmenter_init(
        &fragmenter, update_can_request_id(1U), short_packet,
        sizeof(short_packet)));
    while (update_can_fragmenter_next(&fragmenter, &frame)) {
    }
    TEST_ASSERT_EQUAL_UINT8(8U, frame.dlc);
    TEST_ASSERT_BITS_HIGH(UPDATE_CAN_FRAGMENT_END, frame.data[0]);
    TEST_ASSERT_EQUAL_UINT8(0U, frame.data[2]);
    TEST_ASSERT_EQUAL_UINT8(0U, frame.data[7]);
}

void test_ignores_frame_for_other_node(void)
{
    update_can_reassembly_t reassembly;
    can_frame_t frame = {.id = update_can_request_id(2U), .dlc = 3U,
                         .data = {UPDATE_CAN_FRAGMENT_START, 20U, 0U}};
    const uint8_t *decoded = 0;
    size_t decoded_len = 0U;

    update_can_reassembly_init(&reassembly);
    TEST_ASSERT_EQUAL(
        UPDATE_CAN_REASSEMBLY_NONE,
        update_can_reassembly_feed(&reassembly, &frame,
                                   update_can_request_id(1U), &decoded,
                                   &decoded_len));
}

void test_rejects_out_of_order_fragment(void)
{
    update_can_reassembly_t reassembly;
    const uint8_t *decoded = 0;
    size_t decoded_len = 0U;
    can_frame_t start = {
        .id = update_can_request_id(1U), .dlc = 8U,
        .data = {UPDATE_CAN_FRAGMENT_START, 20U, 0U, 1U, 2U, 3U, 4U, 5U},
    };
    can_frame_t wrong = {
        .id = update_can_request_id(1U), .dlc = 8U,
        .data = {2U, 6U, 7U, 8U, 9U, 10U, 11U, 12U},
    };

    update_can_reassembly_init(&reassembly);
    TEST_ASSERT_EQUAL(UPDATE_CAN_REASSEMBLY_NONE,
                      update_can_reassembly_feed(
                          &reassembly, &start, start.id, &decoded,
                          &decoded_len));
    TEST_ASSERT_EQUAL(UPDATE_CAN_REASSEMBLY_ERROR,
                      update_can_reassembly_feed(
                          &reassembly, &wrong, wrong.id, &decoded,
                          &decoded_len));
}

void test_rejects_bad_declared_length(void)
{
    update_can_reassembly_t reassembly;
    const uint8_t *decoded = 0;
    size_t decoded_len = 0U;
    can_frame_t frame = {
        .id = update_can_request_id(1U), .dlc = 3U,
        .data = {UPDATE_CAN_FRAGMENT_START, 19U, 0U},
    };

    update_can_reassembly_init(&reassembly);
    TEST_ASSERT_EQUAL(UPDATE_CAN_REASSEMBLY_ERROR,
                      update_can_reassembly_feed(
                          &reassembly, &frame, frame.id, &decoded,
                          &decoded_len));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fragments_and_reassembles_maximum_packet);
    RUN_TEST(test_ignores_frame_for_other_node);
    RUN_TEST(test_final_fragment_is_zero_padded_to_dlc_eight);
    RUN_TEST(test_rejects_out_of_order_fragment);
    RUN_TEST(test_rejects_bad_declared_length);
    return UNITY_END();
}

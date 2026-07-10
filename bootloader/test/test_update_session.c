#include "update_session.h"
#include "boot_flash_mock.h"
#include "unity.h"

static boot_update_session_t session;

void setUp(void)
{
    boot_flash_mock_reset();
    boot_update_session_init(&session);
}

void test_identical_written_block_can_be_retried(void)
{
    const uint8_t payload[] = {1U, 2U, 3U};
    const update_packet_t packet = {
        .command = UPDATE_CMD_BLOCK,
        .session_id = 7U,
        .payload_len = sizeof(payload),
        .payload = payload,
    };

    session.session_active = 1U;
    session.session_id = 7U;
    session.expected_image_size = sizeof(payload);
    TEST_ASSERT_EQUAL(UPDATE_STATUS_OK,
                      boot_update_session_process(&session, &packet));
    TEST_ASSERT_EQUAL(UPDATE_STATUS_OK,
                      boot_update_session_process(&session, &packet));
    TEST_ASSERT_EQUAL_UINT32(sizeof(payload), session.received_image_size);
}

void test_changed_duplicate_block_is_rejected(void)
{
    const uint8_t original[] = {1U, 2U, 3U};
    const uint8_t changed[] = {1U, 9U, 3U};
    update_packet_t packet = {
        .command = UPDATE_CMD_BLOCK,
        .session_id = 7U,
        .payload_len = sizeof(original),
        .payload = original,
    };

    session.session_active = 1U;
    session.session_id = 7U;
    session.expected_image_size = sizeof(original);
    TEST_ASSERT_EQUAL(UPDATE_STATUS_OK,
                      boot_update_session_process(&session, &packet));
    packet.payload = changed;
    TEST_ASSERT_EQUAL(UPDATE_STATUS_BAD_SEQUENCE,
                      boot_update_session_process(&session, &packet));
}

void tearDown(void) {}

void test_discovery_does_not_require_transfer_session(void)
{
    const update_packet_t packet = {
        .command = UPDATE_CMD_DISCOVER,
        .session_id = 1U,
    };

    TEST_ASSERT_EQUAL(UPDATE_STATUS_OK,
                      boot_update_session_process(&session, &packet));
}

void test_transfer_command_rejects_different_session_id(void)
{
    const uint8_t payload[] = {1U};
    const update_packet_t packet = {
        .command = UPDATE_CMD_BLOCK,
        .session_id = 0x22222222U,
        .payload_len = sizeof(payload),
        .payload = payload,
    };

    session.session_active = 1U;
    session.session_id = 0x11111111U;
    session.expected_image_size = sizeof(payload);

    TEST_ASSERT_EQUAL(UPDATE_STATUS_BAD_STATE,
                      boot_update_session_process(&session, &packet));
    TEST_ASSERT_EQUAL_UINT32(0U, session.received_image_size);
}

void test_abort_requires_owning_session(void)
{
    const update_packet_t wrong_abort = {
        .command = UPDATE_CMD_ABORT,
        .session_id = 8U,
    };
    const update_packet_t own_abort = {
        .command = UPDATE_CMD_ABORT,
        .session_id = 7U,
    };

    session.session_active = 1U;
    session.session_id = 7U;
    TEST_ASSERT_EQUAL(UPDATE_STATUS_BAD_STATE,
                      boot_update_session_process(&session, &wrong_abort));
    TEST_ASSERT_EQUAL_UINT8(1U, session.session_active);
    TEST_ASSERT_EQUAL(UPDATE_STATUS_OK,
                      boot_update_session_process(&session, &own_abort));
    TEST_ASSERT_EQUAL_UINT8(0U, session.session_active);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_discovery_does_not_require_transfer_session);
    RUN_TEST(test_transfer_command_rejects_different_session_id);
    RUN_TEST(test_abort_requires_owning_session);
    RUN_TEST(test_identical_written_block_can_be_retried);
    RUN_TEST(test_changed_duplicate_block_is_rejected);
    return UNITY_END();
}

#include "boot_policy.h"
#include "image/flash_layout.h"
#include "unity.h"

static boot_state_record_t state;

void setUp(void)
{
    boot_state_init_default(&state);
}

void tearDown(void)
{
}

void test_selects_confirmed_active_slot_by_default(void)
{
    boot_candidate_t candidate = boot_policy_select_candidate(&state);

    TEST_ASSERT_EQUAL_UINT32(BOOT_SLOT_A, candidate.slot);
    TEST_ASSERT_EQUAL_UINT32(APP_SLOT_A_START_ADDR, candidate.image_base);
    TEST_ASSERT_EQUAL_UINT8(0U, candidate.boot_pending);
    TEST_ASSERT_EQUAL_UINT32(0U, state.pending_attempts);
}

void test_selects_pending_slot_and_increments_attempt(void)
{
    boot_policy_mark_slot_pending(&state, BOOT_SLOT_B, 4U, 0x12345678U);

    boot_candidate_t candidate = boot_policy_select_candidate(&state);

    TEST_ASSERT_EQUAL_UINT32(BOOT_SLOT_B, candidate.slot);
    TEST_ASSERT_EQUAL_UINT32(APP_SLOT_B_START_ADDR, candidate.image_base);
    TEST_ASSERT_EQUAL_UINT8(1U, candidate.boot_pending);
    TEST_ASSERT_EQUAL_UINT32(1U, state.pending_attempts);
    TEST_ASSERT_EQUAL_UINT32(4U,
                             state.reserved[BOOT_STATE_PENDING_VERSION_WORD]);
    TEST_ASSERT_EQUAL(BOOT_STATE_VALID, boot_state_validate(&state));
}

void test_exhausted_pending_slot_falls_back_to_active(void)
{
    boot_policy_mark_slot_pending(&state, BOOT_SLOT_B, 4U, 0x12345678U);
    state.pending_attempts = BOOT_STATE_MAX_PENDING_ATTEMPTS;
    boot_state_update_crc(&state);

    boot_candidate_t candidate = boot_policy_select_candidate(&state);

    TEST_ASSERT_EQUAL_UINT32(BOOT_SLOT_A, candidate.slot);
    TEST_ASSERT_EQUAL_UINT8(0U, candidate.boot_pending);
}

void test_marks_pending_slot_bad(void)
{
    boot_policy_mark_slot_pending(&state, BOOT_SLOT_B, 4U, 0x12345678U);
    boot_policy_mark_pending_bad(&state);

    TEST_ASSERT_EQUAL_UINT32(BOOT_STATE_NO_SLOT, state.pending_slot);
    TEST_ASSERT_EQUAL_UINT32(BOOT_SLOT_STATUS_BAD, state.slot_b_status);
    TEST_ASSERT_EQUAL(BOOT_STATE_VALID, boot_state_validate(&state));
}

void test_confirms_pending_slot(void)
{
    boot_policy_mark_slot_pending(&state, BOOT_SLOT_B, 4U, 0x12345678U);
    boot_policy_confirm_pending(&state);

    TEST_ASSERT_EQUAL_UINT32(BOOT_SLOT_B, state.active_slot);
    TEST_ASSERT_EQUAL_UINT32(BOOT_STATE_NO_SLOT, state.pending_slot);
    TEST_ASSERT_EQUAL_UINT32(BOOT_SLOT_STATUS_VALID, state.slot_a_status);
    TEST_ASSERT_EQUAL_UINT32(BOOT_SLOT_STATUS_CONFIRMED, state.slot_b_status);
    TEST_ASSERT_EQUAL_UINT32(4U, state.confirmed_version);
    TEST_ASSERT_EQUAL_HEX32(0x12345678U, state.confirmed_image_crc32);
    TEST_ASSERT_EQUAL(BOOT_STATE_VALID, boot_state_validate(&state));
}

void test_inactive_slot_is_opposite_active_slot(void)
{
    TEST_ASSERT_EQUAL_UINT32(BOOT_SLOT_B, boot_policy_inactive_slot(&state));

    state.active_slot = BOOT_SLOT_B;
    state.slot_a_status = BOOT_SLOT_STATUS_VALID;
    state.slot_b_status = BOOT_SLOT_STATUS_CONFIRMED;
    boot_state_update_crc(&state);

    TEST_ASSERT_EQUAL_UINT32(BOOT_SLOT_A, boot_policy_inactive_slot(&state));
}

void test_inactive_slot_treats_pending_slot_as_running_slot(void)
{
    boot_policy_mark_slot_pending(&state, BOOT_SLOT_B, 4U, 0x12345678U);

    TEST_ASSERT_EQUAL_UINT32(BOOT_SLOT_A, boot_policy_inactive_slot(&state));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_selects_confirmed_active_slot_by_default);
    RUN_TEST(test_selects_pending_slot_and_increments_attempt);
    RUN_TEST(test_exhausted_pending_slot_falls_back_to_active);
    RUN_TEST(test_marks_pending_slot_bad);
    RUN_TEST(test_confirms_pending_slot);
    RUN_TEST(test_inactive_slot_is_opposite_active_slot);
    RUN_TEST(test_inactive_slot_treats_pending_slot_as_running_slot);
    return UNITY_END();
}

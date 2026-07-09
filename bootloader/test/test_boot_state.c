#include "boot/boot_state.h"
#include "unity.h"

static boot_state_record_t state_a;
static boot_state_record_t state_b;

void setUp(void)
{
    boot_state_init_default(&state_a);
    boot_state_init_default(&state_b);
}

void tearDown(void)
{
}

void test_default_state_is_valid_confirmed_slot_a(void)
{
    TEST_ASSERT_EQUAL(BOOT_STATE_VALID, boot_state_validate(&state_a));
    TEST_ASSERT_EQUAL_UINT32(BOOT_SLOT_A, state_a.active_slot);
    TEST_ASSERT_EQUAL_UINT32(BOOT_STATE_NO_SLOT, state_a.pending_slot);
    TEST_ASSERT_EQUAL_UINT32(BOOT_SLOT_STATUS_CONFIRMED,
                             boot_state_status_for_slot(&state_a, BOOT_SLOT_A));
    TEST_ASSERT_EQUAL_UINT32(BOOT_SLOT_STATUS_EMPTY,
                             boot_state_status_for_slot(&state_a, BOOT_SLOT_B));
}

void test_rejects_bad_crc(void)
{
    state_a.active_slot = BOOT_SLOT_B;

    TEST_ASSERT_EQUAL(BOOT_STATE_BAD_CRC, boot_state_validate(&state_a));
}

void test_rejects_invalid_pending_attempt_count(void)
{
    state_a.pending_attempts = BOOT_STATE_MAX_PENDING_ATTEMPTS + 1U;
    boot_state_update_crc(&state_a);

    TEST_ASSERT_EQUAL(BOOT_STATE_BAD_SLOT, boot_state_validate(&state_a));
}

void test_updates_crc_after_state_change(void)
{
    state_a.active_slot = BOOT_SLOT_B;
    state_a.slot_a_status = BOOT_SLOT_STATUS_VALID;
    state_a.slot_b_status = BOOT_SLOT_STATUS_CONFIRMED;
    boot_state_update_crc(&state_a);

    TEST_ASSERT_EQUAL(BOOT_STATE_VALID, boot_state_validate(&state_a));
    TEST_ASSERT_EQUAL_UINT32(BOOT_SLOT_B, state_a.active_slot);
}

void test_selects_latest_valid_generation(void)
{
    boot_state_record_t selected;

    state_b.generation = state_a.generation + 1U;
    state_b.active_slot = BOOT_SLOT_B;
    state_b.slot_a_status = BOOT_SLOT_STATUS_VALID;
    state_b.slot_b_status = BOOT_SLOT_STATUS_CONFIRMED;
    boot_state_update_crc(&state_b);

    TEST_ASSERT_TRUE(boot_state_select_latest(&state_a, &state_b, &selected));
    TEST_ASSERT_EQUAL_UINT32(BOOT_SLOT_B, selected.active_slot);
    TEST_ASSERT_EQUAL_UINT32(state_b.generation, selected.generation);
}

void test_selects_only_valid_copy(void)
{
    boot_state_record_t selected;

    state_b.magic = 0U;

    TEST_ASSERT_TRUE(boot_state_select_latest(&state_a, &state_b, &selected));
    TEST_ASSERT_EQUAL_UINT32(BOOT_SLOT_A, selected.active_slot);
}

void test_rejects_when_no_valid_copy_exists(void)
{
    boot_state_record_t selected;

    state_a.magic = 0U;
    state_b.magic = 0U;

    TEST_ASSERT_FALSE(boot_state_select_latest(&state_a, &state_b, &selected));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_default_state_is_valid_confirmed_slot_a);
    RUN_TEST(test_rejects_bad_crc);
    RUN_TEST(test_rejects_invalid_pending_attempt_count);
    RUN_TEST(test_updates_crc_after_state_change);
    RUN_TEST(test_selects_latest_valid_generation);
    RUN_TEST(test_selects_only_valid_copy);
    RUN_TEST(test_rejects_when_no_valid_copy_exists);
    return UNITY_END();
}

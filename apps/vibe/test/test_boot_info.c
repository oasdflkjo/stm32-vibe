#include "platform/boot_info.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

void test_exposes_validated_boot_information(void)
{
    boot_handoff_t handoff = {
        .magic = BOOT_HANDOFF_MAGIC,
        .version = BOOT_HANDOFF_VERSION,
        .size = sizeof(handoff),
        .image_base = 0x08048000U,
        .image_size = 4096U,
        .slot = 1U,
        .state = BOOT_HANDOFF_PENDING,
        .boot_attempt = 2U,
    };
    platform_boot_info_t info;

    boot_handoff_update_crc(&handoff);
    TEST_ASSERT_TRUE(platform_boot_info_init_from(&handoff));
    TEST_ASSERT_TRUE(platform_boot_info_get(&info));
    TEST_ASSERT_EQUAL_UINT32(1U, info.slot);
    TEST_ASSERT_TRUE(info.pending);
    TEST_ASSERT_EQUAL_UINT32(2U, info.boot_attempt);
}

void test_rejects_corrupt_handoff(void)
{
    boot_handoff_t handoff = {0};
    platform_boot_info_t info;

    TEST_ASSERT_FALSE(platform_boot_info_init_from(&handoff));
    TEST_ASSERT_FALSE(platform_boot_info_get(&info));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_exposes_validated_boot_information);
    RUN_TEST(test_rejects_corrupt_handoff);
    return UNITY_END();
}

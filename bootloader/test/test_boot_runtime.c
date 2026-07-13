#include "boot/boot_runtime.h"
#include "unity.h"

static boot_handoff_t handoff;
static uint32_t image[16];
static uint32_t vectors[8];
static uint32_t got[4];

void setUp(void)
{
    handoff = (boot_handoff_t){
        .magic = BOOT_HANDOFF_MAGIC,
        .version = BOOT_HANDOFF_VERSION,
        .size = sizeof(handoff),
        .image_base = 0x08011000U,
        .image_size = 4096U,
        .slot = 0U,
        .state = BOOT_HANDOFF_PENDING,
        .boot_attempt = 1U,
    };
    boot_handoff_update_crc(&handoff);
    for (uint32_t index = 0U; index < 16U; index++) {
        image[index] = 0U;
    }
}

void test_relocates_vectors_and_flash_got_entries(void)
{
    image[0] = 0x20013800U;
    image[1] = 0x21U;
    image[2] = 0U;
    image[3] = 0x31U;
    image[8] = 0x20U;
    image[9] = 0x20000100U;

    TEST_ASSERT_TRUE(boot_runtime_relocate(
        (const uint8_t *)image, 0x08011000U, sizeof(image), 4U, 32U, 8U,
        vectors, 8U, got, 4U));
    TEST_ASSERT_EQUAL_HEX32(0x20013800U, vectors[0]);
    TEST_ASSERT_EQUAL_HEX32(0x08011021U, vectors[1]);
    TEST_ASSERT_EQUAL_HEX32(0U, vectors[2]);
    TEST_ASSERT_EQUAL_HEX32(0x08011031U, vectors[3]);
    TEST_ASSERT_EQUAL_HEX32(0x08011020U, got[0]);
    TEST_ASSERT_EQUAL_HEX32(0x20000100U, got[1]);
}

void test_rejects_invalid_vector_and_got_bounds(void)
{
    image[0] = 0x20013800U;
    image[1] = 0x20U;
    TEST_ASSERT_FALSE(boot_runtime_relocate(
        (const uint8_t *)image, 0x08011000U, sizeof(image), 2U, 32U, 8U,
        vectors, 8U, got, 4U));
    image[1] = 0x21U;
    TEST_ASSERT_FALSE(boot_runtime_relocate(
        (const uint8_t *)image, 0x08011000U, sizeof(image), 2U, 60U, 8U,
        vectors, 8U, got, 4U));
}

void tearDown(void) {}

void test_accepts_valid_handoff(void)
{
    TEST_ASSERT_TRUE(boot_handoff_is_valid(&handoff));
}

void test_rejects_corrupted_handoff(void)
{
    handoff.image_base ^= 1U;
    TEST_ASSERT_FALSE(boot_handoff_is_valid(&handoff));
}

void test_rejects_invalid_slot_and_state(void)
{
    handoff.slot = 2U;
    boot_handoff_update_crc(&handoff);
    TEST_ASSERT_FALSE(boot_handoff_is_valid(&handoff));
    handoff.slot = 0U;
    handoff.state = 2U;
    boot_handoff_update_crc(&handoff);
    TEST_ASSERT_FALSE(boot_handoff_is_valid(&handoff));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_accepts_valid_handoff);
    RUN_TEST(test_rejects_corrupted_handoff);
    RUN_TEST(test_rejects_invalid_slot_and_state);
    RUN_TEST(test_relocates_vectors_and_flash_got_entries);
    RUN_TEST(test_rejects_invalid_vector_and_got_bounds);
    return UNITY_END();
}

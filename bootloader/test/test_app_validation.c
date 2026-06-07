#include "app_image.h"
#include "app_validation.h"
#include "image/app_manifest.h"
#include "unity.h"
#include <stddef.h>
#include <string.h>

#define TEST_IMAGE_SIZE 0x220U

_Alignas(4) static uint8_t test_image[TEST_IMAGE_SIZE];

static uint32_t test_crc32(void)
{
    const uint32_t crc_offset =
        APP_MANIFEST_OFFSET + offsetof(app_manifest_t, image_crc32);
    uint32_t crc = UINT32_MAX;

    for (uint32_t index = 0U; index < sizeof(test_image); index++) {
        uint8_t value = test_image[index];

        if ((index >= crc_offset) &&
            (index < crc_offset + sizeof(uint32_t))) {
            value = 0U;
        }

        crc ^= value;
        for (uint32_t bit = 0U; bit < 8U; bit++) {
            crc = (crc & 1U) != 0U
                      ? (crc >> 1U) ^ 0xEDB88320U
                      : crc >> 1U;
        }
    }

    return crc ^ UINT32_MAX;
}

static app_manifest_t *test_manifest(void)
{
    return (app_manifest_t *)(test_image + APP_MANIFEST_OFFSET);
}

static void prepare_valid_image(void)
{
    app_manifest_t *manifest;

    memset(test_image, 0, sizeof(test_image));
    manifest = test_manifest();
    manifest->magic = APP_MANIFEST_MAGIC;
    manifest->image_size = sizeof(test_image);
    manifest->version = 7U;
    manifest->image_crc32 = test_crc32();
}

void setUp(void)
{
    prepare_valid_image();
}

void tearDown(void)
{
}

void test_accepts_valid_application_vectors(void)
{
    TEST_ASSERT_TRUE(
        app_vectors_are_valid(RAM_END_ADDR, APP_START_ADDR + 0x101U));
}

void test_rejects_stack_outside_ram(void)
{
    TEST_ASSERT_FALSE(
        app_vectors_are_valid(RAM_END_ADDR + 8U, APP_START_ADDR + 0x101U));
}

void test_rejects_misaligned_stack(void)
{
    TEST_ASSERT_FALSE(
        app_vectors_are_valid(RAM_END_ADDR - 4U, APP_START_ADDR + 0x101U));
}

void test_rejects_reset_handler_outside_app(void)
{
    TEST_ASSERT_FALSE(
        app_vectors_are_valid(RAM_END_ADDR, APP_START_ADDR - 1U));
}

void test_rejects_reset_handler_without_thumb_bit(void)
{
    TEST_ASSERT_FALSE(
        app_vectors_are_valid(RAM_END_ADDR, APP_START_ADDR + 0x100U));
}

void test_accepts_image_with_valid_manifest_and_crc(void)
{
    app_image_result_t result =
        app_image_validate(test_image, sizeof(test_image));

    TEST_ASSERT_EQUAL(APP_IMAGE_VALID, result.status);
    TEST_ASSERT_EQUAL_UINT32(7U, result.version);
    TEST_ASSERT_EQUAL_UINT32(sizeof(test_image), result.image_size);
    TEST_ASSERT_EQUAL_HEX32(result.expected_crc32, result.calculated_crc32);
}

void test_rejects_image_with_bad_magic(void)
{
    test_manifest()->magic = 0U;

    TEST_ASSERT_EQUAL(
        APP_IMAGE_BAD_MAGIC,
        app_image_validate(test_image, sizeof(test_image)).status);
}

void test_rejects_image_with_invalid_size(void)
{
    test_manifest()->image_size = sizeof(test_image) + 1U;

    TEST_ASSERT_EQUAL(
        APP_IMAGE_BAD_SIZE,
        app_image_validate(test_image, sizeof(test_image)).status);
}

void test_rejects_buffer_too_short_for_manifest(void)
{
    TEST_ASSERT_EQUAL(
        APP_IMAGE_BAD_SIZE,
        app_image_validate(test_image, APP_MANIFEST_OFFSET).status);
}

void test_rejects_corrupted_image(void)
{
    test_image[32] ^= 0x01U;

    TEST_ASSERT_EQUAL(
        APP_IMAGE_BAD_CRC,
        app_image_validate(test_image, sizeof(test_image)).status);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_accepts_valid_application_vectors);
    RUN_TEST(test_rejects_stack_outside_ram);
    RUN_TEST(test_rejects_misaligned_stack);
    RUN_TEST(test_rejects_reset_handler_outside_app);
    RUN_TEST(test_rejects_reset_handler_without_thumb_bit);
    RUN_TEST(test_accepts_image_with_valid_manifest_and_crc);
    RUN_TEST(test_rejects_image_with_bad_magic);
    RUN_TEST(test_rejects_image_with_invalid_size);
    RUN_TEST(test_rejects_buffer_too_short_for_manifest);
    RUN_TEST(test_rejects_corrupted_image);
    return UNITY_END();
}

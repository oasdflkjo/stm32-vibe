#include "app_image.h"
#include "image/app_manifest.h"
#include <stddef.h>

static uint32_t crc32_update(uint32_t crc, uint8_t value)
{
    crc ^= value;

    for (uint32_t bit = 0U; bit < 8U; bit++) {
        crc = (crc & 1U) != 0U
                  ? (crc >> 1U) ^ 0xEDB88320U
                  : crc >> 1U;
    }

    return crc;
}

static uint32_t image_crc32(const uint8_t *image, uint32_t image_size)
{
    const uint32_t crc_offset =
        APP_MANIFEST_OFFSET + offsetof(app_manifest_t, image_crc32);
    uint32_t crc = UINT32_MAX;

    for (uint32_t index = 0U; index < image_size; index++) {
        uint8_t value = image[index];

        if ((index >= crc_offset) &&
            (index < crc_offset + sizeof(uint32_t))) {
            value = 0U;
        }

        crc = crc32_update(crc, value);
    }

    return crc ^ UINT32_MAX;
}

app_image_result_t app_image_validate(const uint8_t *image,
                                      uint32_t capacity)
{
    app_image_result_t result = {
        .status = APP_IMAGE_BAD_SIZE,
    };
    const app_manifest_t *manifest;

    if (capacity < APP_MANIFEST_OFFSET + sizeof(*manifest)) {
        return result;
    }

    manifest = (const app_manifest_t *)(image + APP_MANIFEST_OFFSET);
    result.status = APP_IMAGE_BAD_MAGIC;
    result.version = manifest->version;
    result.image_size = manifest->image_size;
    result.expected_crc32 = manifest->image_crc32;

    if (manifest->magic != APP_MANIFEST_MAGIC) {
        return result;
    }

    if ((manifest->image_size < APP_MANIFEST_OFFSET + sizeof(*manifest)) ||
        (manifest->image_size > capacity)) {
        result.status = APP_IMAGE_BAD_SIZE;
        return result;
    }

    result.calculated_crc32 = image_crc32(image, manifest->image_size);
    if (result.calculated_crc32 != manifest->image_crc32) {
        result.status = APP_IMAGE_BAD_CRC;
        return result;
    }

    result.status = APP_IMAGE_VALID;
    return result;
}

#pragma once

#include <stdint.h>

typedef enum {
    APP_IMAGE_VALID = 0,
    APP_IMAGE_BAD_MAGIC = 1,
    APP_IMAGE_BAD_SIZE = 2,
    APP_IMAGE_BAD_CRC = 3,
} app_image_status_t;

typedef struct {
    app_image_status_t status;
    uint32_t version;
    uint32_t image_size;
    uint32_t expected_crc32;
    uint32_t calculated_crc32;
} app_image_result_t;

app_image_result_t app_image_validate(const uint8_t *image,
                                      uint32_t capacity);

#pragma once

#include <stdint.h>

#define APP_IMAGE_START_ADDR 0x08004000U
#define APP_IMAGE_END_ADDR   0x08080000U
#define APP_IMAGE_CAPACITY   (APP_IMAGE_END_ADDR - APP_IMAGE_START_ADDR)

#define APP_MANIFEST_OFFSET 0x200U
#define APP_MANIFEST_MAGIC  0x45424956U

typedef struct {
    uint32_t magic;
    uint32_t image_size;
    uint32_t image_crc32;
    uint32_t version;
} app_manifest_t;

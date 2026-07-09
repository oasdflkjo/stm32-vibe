#pragma once

#include <stdint.h>

#define APP_IMAGE_START_ADDR 0x08004000U
#define APP_IMAGE_END_ADDR   0x08080000U
#define APP_IMAGE_CAPACITY   (APP_IMAGE_END_ADDR - APP_IMAGE_START_ADDR)

#define APP_MANIFEST_OFFSET 0x200U
#define APP_MANIFEST_MAGIC  0x45424956U
#define APP_MANIFEST_VERSION 2U
#define APP_MANIFEST_RESERVED_WORDS 8U

typedef struct {
    uint32_t magic;
    uint32_t manifest_version;
    uint32_t manifest_size;
    uint32_t image_size;
    uint32_t image_crc32;
    uint32_t software_version;
    uint32_t hardware_id;
    uint32_t image_flags;
    uint32_t reserved[APP_MANIFEST_RESERVED_WORDS];
} app_manifest_t;

#define APP_MANIFEST_SIZE ((uint32_t)sizeof(app_manifest_t))

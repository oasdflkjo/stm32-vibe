#pragma once

#include "image/flash_layout.h"
#include <stdint.h>

#define APP_IMAGE_START_ADDR APP_SLOT_A_START_ADDR
#define APP_IMAGE_END_ADDR   APP_SLOT_A_END_ADDR
#define APP_IMAGE_CAPACITY   APP_SLOT_SIZE

#define APP_MANIFEST_OFFSET 0x200U
#define APP_MANIFEST_MAGIC  0x45424956U
#define APP_MANIFEST_VERSION 2U
#define APP_MANIFEST_RESERVED_WORDS 8U
#define APP_MANIFEST_APP_ID_WORD 0U
#define APP_MANIFEST_BOARD_ID_WORD 1U
#define APP_MANIFEST_VECTOR_WORDS_WORD 2U
#define APP_MANIFEST_GOT_OFFSET_WORD 3U
#define APP_MANIFEST_GOT_SIZE_WORD 4U
#define APP_MANIFEST_DATA_LOAD_OFFSET_WORD 5U

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

#define app_manifest_app_id(manifest) \
    ((manifest)->reserved[APP_MANIFEST_APP_ID_WORD])
#define app_manifest_board_id(manifest) \
    ((manifest)->reserved[APP_MANIFEST_BOARD_ID_WORD])
#define app_manifest_vector_words(manifest) \
    ((manifest)->reserved[APP_MANIFEST_VECTOR_WORDS_WORD])
#define app_manifest_got_offset(manifest) \
    ((manifest)->reserved[APP_MANIFEST_GOT_OFFSET_WORD])
#define app_manifest_got_size(manifest) \
    ((manifest)->reserved[APP_MANIFEST_GOT_SIZE_WORD])
#define app_manifest_data_load_offset(manifest) \
    ((manifest)->reserved[APP_MANIFEST_DATA_LOAD_OFFSET_WORD])
#define app_manifest_is_relocatable(manifest) \
    (app_manifest_vector_words(manifest) >= 2U && \
     app_manifest_vector_words(manifest) <= 128U)

#define APP_MANIFEST_SIZE ((uint32_t)sizeof(app_manifest_t))

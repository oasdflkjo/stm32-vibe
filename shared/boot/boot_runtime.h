#pragma once

#include <stdint.h>

#if defined(STM32F446xx)
#define BOOT_RUNTIME_GOT_ADDR 0x2001F800U
#define BOOT_RUNTIME_VECTOR_ADDR 0x2001FC00U
#define BOOT_HANDOFF_ADDR 0x2001FE00U
#define BOOT_UPDATE_REQUEST_ADDR 0x2001FFF0U
#else
#define BOOT_RUNTIME_GOT_ADDR 0x20013800U
#define BOOT_RUNTIME_VECTOR_ADDR 0x20013C00U
#define BOOT_HANDOFF_ADDR 0x20013E00U
#define BOOT_UPDATE_REQUEST_ADDR 0x20013FF0U
#endif

#define BOOT_RUNTIME_VECTOR_SIZE 0x00000200U
#define BOOT_RUNTIME_VECTOR_WORDS \
    (BOOT_RUNTIME_VECTOR_SIZE / sizeof(uint32_t))

#define BOOT_RUNTIME_GOT_SIZE 0x00000400U
#define BOOT_RUNTIME_GOT_WORDS (BOOT_RUNTIME_GOT_SIZE / sizeof(uint32_t))

#define BOOT_HANDOFF_MAGIC 0x46444E48U
#define BOOT_HANDOFF_VERSION 1U

typedef enum {
    BOOT_HANDOFF_CONFIRMED = 0,
    BOOT_HANDOFF_PENDING = 1,
} boot_handoff_state_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t image_base;
    uint32_t image_size;
    uint32_t slot;
    uint32_t state;
    uint32_t boot_attempt;
    uint32_t crc32;
} boot_handoff_t;

uint32_t boot_handoff_crc32(const boot_handoff_t *handoff);
void boot_handoff_update_crc(boot_handoff_t *handoff);
int boot_handoff_is_valid(const boot_handoff_t *handoff);

int boot_runtime_relocate(const uint8_t *image,
                          uint32_t image_base,
                          uint32_t image_size,
                          uint32_t vector_words,
                          uint32_t got_offset,
                          uint32_t got_size,
                          uint32_t *vectors,
                          uint32_t vector_capacity_words,
                          uint32_t *got,
                          uint32_t got_capacity_words);

static inline volatile boot_handoff_t *boot_handoff_shared(void)
{
    return (volatile boot_handoff_t *)BOOT_HANDOFF_ADDR;
}

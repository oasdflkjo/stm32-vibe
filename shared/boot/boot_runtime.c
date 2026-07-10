#include "boot/boot_runtime.h"

#include <stddef.h>

uint32_t boot_handoff_crc32(const boot_handoff_t *handoff)
{
    const uint8_t *bytes = (const uint8_t *)handoff;
    const size_t crc_offset = offsetof(boot_handoff_t, crc32);
    uint32_t crc = UINT32_MAX;

    if (handoff == 0) {
        return 0U;
    }
    for (size_t index = 0U; index < sizeof(*handoff); index++) {
        uint8_t value = bytes[index];

        if ((index >= crc_offset) &&
            (index < crc_offset + sizeof(handoff->crc32))) {
            value = 0U;
        }
        crc ^= value;
        for (uint32_t bit = 0U; bit < 8U; bit++) {
            crc = (crc & 1U) != 0U ? (crc >> 1U) ^ 0xEDB88320U
                                   : crc >> 1U;
        }
    }
    return crc ^ UINT32_MAX;
}

void boot_handoff_update_crc(boot_handoff_t *handoff)
{
    if (handoff != 0) {
        handoff->crc32 = 0U;
        handoff->crc32 = boot_handoff_crc32(handoff);
    }
}

int boot_handoff_is_valid(const boot_handoff_t *handoff)
{
    if ((handoff == 0) || (handoff->magic != BOOT_HANDOFF_MAGIC) ||
        (handoff->version != BOOT_HANDOFF_VERSION) ||
        (handoff->size != sizeof(*handoff)) ||
        ((handoff->slot != 0U) && (handoff->slot != 1U)) ||
        ((handoff->state != BOOT_HANDOFF_CONFIRMED) &&
         (handoff->state != BOOT_HANDOFF_PENDING)) ||
        (handoff->image_size == 0U)) {
        return 0;
    }
    return handoff->crc32 == boot_handoff_crc32(handoff);
}

int boot_runtime_relocate(const uint8_t *image,
                          uint32_t image_base,
                          uint32_t image_size,
                          uint32_t vector_words,
                          uint32_t got_offset,
                          uint32_t got_size,
                          uint32_t *vectors,
                          uint32_t vector_capacity_words,
                          uint32_t *got,
                          uint32_t got_capacity_words)
{
    const uint32_t *stored_vectors = (const uint32_t *)image;
    const uint32_t *stored_got;
    uint32_t got_words;

    if ((image == 0) || (vectors == 0) || (got == 0) ||
        (image_size < (2U * sizeof(uint32_t))) || (vector_words < 2U) ||
        (vector_words > vector_capacity_words) ||
        (vector_words > BOOT_RUNTIME_VECTOR_WORDS) ||
        (vector_words > (image_size / sizeof(uint32_t))) ||
        ((got_offset & 3U) != 0U) || ((got_size & 3U) != 0U) ||
        (got_offset > image_size) || (got_size > (image_size - got_offset))) {
        return 0;
    }

    got_words = got_size / sizeof(uint32_t);
    if ((got_words > got_capacity_words) ||
        (got_words > BOOT_RUNTIME_GOT_WORDS)) {
        return 0;
    }

    vectors[0] = stored_vectors[0];
    for (uint32_t index = 1U; index < vector_words; index++) {
        uint32_t handler = stored_vectors[index];

        if ((handler == 0U) || (handler == 0xF108F85FU)) {
            vectors[index] = 0U;
        } else if (((handler & 1U) == 0U) || ((handler & ~1U) >= image_size)) {
            return 0;
        } else {
            vectors[index] = image_base + handler;
        }
    }

    stored_got = (const uint32_t *)&image[got_offset];
    for (uint32_t index = 0U; index < got_words; index++) {
        uint32_t value = stored_got[index];

        got[index] = value < image_size ? image_base + value : value;
    }
    return 1;
}

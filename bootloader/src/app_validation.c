#include "app_validation.h"

int app_vectors_are_valid_for_slot(uint32_t stack_pointer,
                                   uint32_t reset_handler,
                                   uint32_t slot_start,
                                   uint32_t slot_end)
{
    uint32_t reset_address = reset_handler & ~1U;

    if ((stack_pointer < RAM_START_ADDR) || (stack_pointer > RAM_END_ADDR) ||
        ((stack_pointer & 0x7U) != 0U)) {
        return 0;
    }

    return ((reset_handler & 1U) != 0U) &&
           (reset_address >= slot_start) &&
           (reset_address < slot_end);
}

int app_relative_vectors_are_valid(uint32_t stack_pointer,
                                   uint32_t reset_offset,
                                   uint32_t image_size)
{
    if ((stack_pointer < RAM_START_ADDR) ||
        (stack_pointer > BOOT_RUNTIME_GOT_ADDR) ||
        ((stack_pointer & 0x7U) != 0U)) {
        return 0;
    }
    return ((reset_offset & 1U) != 0U) &&
           ((reset_offset & ~1U) < image_size);
}

int app_vectors_are_valid(uint32_t stack_pointer, uint32_t reset_handler)
{
    return app_vectors_are_valid_for_slot(stack_pointer, reset_handler,
                                          APP_START_ADDR, APP_END_ADDR);
}

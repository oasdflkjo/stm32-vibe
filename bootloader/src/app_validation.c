#include "app_validation.h"

int app_vectors_are_valid(uint32_t stack_pointer, uint32_t reset_handler)
{
    uint32_t reset_address = reset_handler & ~1U;

    if ((stack_pointer < RAM_START_ADDR) || (stack_pointer > RAM_END_ADDR) ||
        ((stack_pointer & 0x7U) != 0U)) {
        return 0;
    }

    return ((reset_handler & 1U) != 0U) &&
           (reset_address >= APP_START_ADDR) &&
           (reset_address < APP_END_ADDR);
}

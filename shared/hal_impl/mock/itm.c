#include "hal/itm.h"
#include "hal/itm_mock.h"

#define ITM_MOCK_CAPACITY 256U

static uint8_t output[ITM_MOCK_CAPACITY];
static size_t output_size;

void itm_init(uint32_t cpu_hz, uint32_t swo_baud)
{
    (void)cpu_hz;
    (void)swo_baud;
}

void itm_putchar(uint8_t ch)
{
    if (output_size < ITM_MOCK_CAPACITY) {
        output[output_size] = ch;
        output_size++;
    }
}

void itm_puts(const char *s)
{
    while (*s != '\0') {
        itm_putchar((uint8_t)*s);
        s++;
    }
}

void itm_mock_reset(void)
{
    output_size = 0U;
}

size_t itm_mock_size(void)
{
    return output_size;
}

uint8_t itm_mock_byte(size_t index)
{
    return index < output_size ? output[index] : 0U;
}

#pragma once

#include <stddef.h>
#include <stdint.h>

void itm_mock_reset(void);
size_t itm_mock_size(void);
uint8_t itm_mock_byte(size_t index);

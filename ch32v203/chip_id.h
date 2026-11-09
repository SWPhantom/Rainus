#ifndef RAINUS_CHIP_ID_H
#define RAINUS_CHIP_ID_H
#include <stdint.h>

#define CHIP_ID_BYTES 12
#define CHIP_ID_TEXT_LEN (CHIP_ID_BYTES * 2)

// Format in ascending memory-address byte order, preserving leading zeroes.
void chip_id_format(const uint8_t bytes[CHIP_ID_BYTES], char out[CHIP_ID_TEXT_LEN + 1]);
#endif

#ifndef RAINUS_CHIP_ID_H
#define RAINUS_CHIP_ID_H
#include <stdint.h>

#define CHIP_ID_BYTES 12
#define CHIP_ID_TEXT_LEN (CHIP_ID_BYTES * 2)

// Convert the chip's 12 ID bytes into 24 hex characters and a final '\0'.
// Keep ascending memory-address order and leading zeroes; this assigns no ID.
void chip_id_format(const uint8_t bytes[CHIP_ID_BYTES], char out[CHIP_ID_TEXT_LEN + 1]);
#endif

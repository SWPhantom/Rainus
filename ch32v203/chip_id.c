#include "chip_id.h"

void chip_id_format(const uint8_t bytes[CHIP_ID_BYTES], char out[CHIP_ID_TEXT_LEN + 1])
{
    static const char digits[] = "0123456789ABCDEF";
    for (unsigned i = 0; i < CHIP_ID_BYTES; ++i) {
        out[2 * i] = digits[bytes[i] >> 4];
        out[2 * i + 1] = digits[bytes[i] & 15];
    }
    out[CHIP_ID_TEXT_LEN] = '\0';
}

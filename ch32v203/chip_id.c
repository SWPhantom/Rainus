#include "chip_id.h"

void chip_id_format(const uint8_t bytes[CHIP_ID_BYTES], char out[CHIP_ID_TEXT_LEN + 1])
{
    static const char digits[] = "0123456789ABCDEF";
    for (unsigned i = 0; i < CHIP_ID_BYTES; ++i) {
        // Each byte becomes two hexadecimal characters. For example, 0xAB
        // becomes 'A' and 'B': >> 4 selects the upper four bits (10), and
        // & 0x0F selects the lower four bits (11). Use those as digit indexes.
        out[2 * i] = digits[bytes[i] >> 4];
        out[2 * i + 1] = digits[bytes[i] & 0x0F];
    }
    out[CHIP_ID_TEXT_LEN] = '\0';
}

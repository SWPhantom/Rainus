#include "chip_id.h"
#include <stdio.h>

void chip_id_format(const uint8_t bytes[CHIP_ID_BYTES], char out[CHIP_ID_TEXT_LEN + 1])
{
    for (unsigned i = 0; i < CHIP_ID_BYTES; ++i) {
        // %02X prints a byte as two uppercase hex digits, including a leading
        // zero. The 3-byte slice holds those digits and '\0'; the next byte's
        // digits replace that terminator. The last iteration terminates the ID.
        snprintf(out + 2 * i, 3, "%02X", (unsigned)bytes[i]);
    }
}

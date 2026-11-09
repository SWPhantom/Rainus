#include "../chip_id.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    const uint8_t bytes[CHIP_ID_BYTES] = {0x00, 0x01, 0x09, 0x0a, 0x10, 0x2b,
                                          0x34, 0x56, 0x78, 0x9a, 0xbc, 0xff};
    char out[CHIP_ID_TEXT_LEN + 1];
    chip_id_format(bytes, out);
    assert(strcmp(out, "0001090A102B3456789ABCFF") == 0);
    puts("Chip ID byte-order and formatting test passed");
}

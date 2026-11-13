// Temporary read-only SD initialization / FAT mount diagnostic.
#include "ch32fun.h"
#include "fatfs/ff.h"

extern int sd_init_diagnostic_stage;
extern int sd_cmd0_diagnostic_detail;
extern uint8_t sd_cmd0_response;
static FATFS filesystem;

static void repeat_flashes(unsigned count)
{
    for (;;) {
        for (unsigned i = 0; i < count; ++i) {
            funDigitalWrite(PA15, FUN_LOW);
            Delay_Ms(180);
            funDigitalWrite(PA15, FUN_HIGH);
            Delay_Ms(250);
        }
        Delay_Ms(2200);
    }
}

int main(void)
{
    SystemInit();
    funGpioInitAll();
    funDigitalWrite(PA15, FUN_HIGH);
    funPinMode(PA15, GPIO_CFGLR_OUT_10Mhz_PP);
    // Immediate mount initializes the card and reads the volume; no writes.
    FRESULT result = f_mount(&filesystem, "", 1);
    if (result == FR_OK) repeat_flashes(2);
    if (result == FR_NOT_READY) {
        if (sd_init_diagnostic_stage == 3) {
            if (sd_cmd0_diagnostic_detail == 4) repeat_flashes(8);
            if (sd_cmd0_response == 0x00) repeat_flashes(3);
            if (sd_cmd0_response == 0x05) repeat_flashes(4);
            if (sd_cmd0_response == 0x04) repeat_flashes(5);
            if (sd_cmd0_response == 0x7f) repeat_flashes(6);
            repeat_flashes(7);
        }
        repeat_flashes(sd_init_diagnostic_stage + 2);
    }
    if (result == FR_DISK_ERR) repeat_flashes(11);
    if (result == FR_NO_FILESYSTEM) repeat_flashes(12);
    repeat_flashes(13);
}

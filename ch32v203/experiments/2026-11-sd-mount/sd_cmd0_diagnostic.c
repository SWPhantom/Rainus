// Temporary read-only SD initialization / FAT mount diagnostic.
#include "ch32fun.h"
#include "fatfs/ff.h"

extern int sd_init_diagnostic_stage;
extern int sd_cmd0_diagnostic_detail;
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
        if (sd_init_diagnostic_stage == 3) repeat_flashes(sd_cmd0_diagnostic_detail);
        repeat_flashes(sd_init_diagnostic_stage + 2);
    }
    if (result == FR_DISK_ERR) repeat_flashes(11);
    if (result == FR_NO_FILESYSTEM) repeat_flashes(12);
    repeat_flashes(13);
}

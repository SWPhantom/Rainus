// SDv2/SDHC over the nanoCH32V203 v1.1 onboard SPI1 socket.
#include "ch32fun.h"
#include "fatfs/ff.h"
#include "fatfs/diskio.h"

static DSTATUS status = STA_NOINIT;
static int block_addressing;
volatile unsigned sd_debug_step;
volatile unsigned sd_debug_response;

static uint8_t transfer(uint8_t b)
{
    while (!(SPI1->STATR & SPI_STATR_TXE)) {}
    SPI1->DATAR = b;
    while (!(SPI1->STATR & SPI_STATR_RXNE)) {}
    return SPI1->DATAR;
}

static void deselect(void)
{
    funDigitalWrite(PA3, FUN_HIGH);
    transfer(0xff);
}

static int ready(unsigned timeout_ms)
{
    do {
        if (transfer(0xff) == 0xff) return 1;
        Delay_Ms(1);
    } while (timeout_ms--);
    return 0;
}

static int select_card(void)
{
    funDigitalWrite(PA3, FUN_LOW);
    transfer(0xff);
    if (ready(500)) return 1;
    deselect();
    return 0;
}

static uint8_t command(uint8_t cmd, uint32_t arg)
{
    uint8_t response;
    deselect();
    if (!select_card()) return 0xff;
    transfer(0x40 | cmd);
    transfer(arg >> 24); transfer(arg >> 16); transfer(arg >> 8); transfer(arg);
    transfer(cmd == 0 ? 0x95 : cmd == 8 ? 0x87 : 0xff);
    for (int i = 0; i < 10; ++i) {
        response = transfer(0xff);
        if (!(response & 0x80)) return response;
    }
    return 0xff;
}

DSTATUS disk_initialize(BYTE drive)
{
    if (drive != 0) return STA_NOINIT;
    status = STA_NOINIT;
    block_addressing = 0;
    RCC->APB2PCENR |= RCC_APB2Periph_SPI1;
    funDigitalWrite(PA3, FUN_HIGH);
    funPinMode(PA3, GPIO_CFGLR_OUT_10Mhz_PP);
    funPinMode(PA5, GPIO_CFGLR_OUT_10Mhz_AF_PP);
    funPinMode(PA6, GPIO_CFGLR_IN_FLOAT);
    funPinMode(PA7, GPIO_CFGLR_OUT_10Mhz_AF_PP);
    // 48 MHz APB2 / 256 = 187.5 kHz, below SD's 400 kHz init limit.
    SPI1->CTLR1 = SPI_CTLR1_MSTR | SPI_CTLR1_SSM | SPI_CTLR1_SSI |
                  SPI_CTLR1_BR | SPI_CTLR1_SPE;
    for (int i = 0; i < 10; ++i) transfer(0xff);
    sd_debug_step = 0;
    sd_debug_response = command(0, 0);
    if (sd_debug_response != 1) goto done;
    sd_debug_step = 8;
    sd_debug_response = command(8, 0x1aa);
    if (sd_debug_response != 1) goto done;
    uint8_t r7[4];
    for (int i = 0; i < 4; ++i) r7[i] = transfer(0xff);
    if (r7[2] != 1 || r7[3] != 0xaa) { sd_debug_response = r7[3]; goto done; }
    int initialized = 0;
    for (int i = 0; i < 1000; ++i) {
        sd_debug_step = 55;
        sd_debug_response = command(55, 0);
        if (sd_debug_response > 1) break;
        sd_debug_step = 41;
        sd_debug_response = command(41, 0x40000000);
        if (sd_debug_response == 0) { initialized = 1; break; }
        Delay_Ms(1);
    }
    if (!initialized) goto done;
    sd_debug_step = 58;
    sd_debug_response = command(58, 0);
    if (sd_debug_response != 0) goto done;
    uint8_t ocr = transfer(0xff);
    for (int i = 0; i < 3; ++i) transfer(0xff);
    block_addressing = !!(ocr & 0x40);
    if (!block_addressing) {
        sd_debug_step = 16;
        sd_debug_response = command(16, 512);
        if (sd_debug_response != 0) goto done;
    }
    status = 0;
    sd_debug_step = 99;
    // 48 MHz / 32 = 1.5 MHz for data transfers.
    SPI1->CTLR1 = SPI_CTLR1_MSTR | SPI_CTLR1_SSM | SPI_CTLR1_SSI |
                  SPI_CTLR1_BR_2 | SPI_CTLR1_SPE;
done:
    deselect();
    return status;
}

DSTATUS disk_status(BYTE drive) { return drive == 0 ? status : STA_NOINIT; }

DRESULT disk_read(BYTE drive, BYTE *buffer, LBA_t sector, UINT count)
{
    if (drive || (status & STA_NOINIT) || !count) return RES_NOTRDY;
    while (count--) {
        if (command(17, block_addressing ? sector : sector * 512) != 0) goto fail;
        int token = 0;
        for (int i = 0; i < 250; ++i) {
            token = transfer(0xff);
            if (token != 0xff) break;
            Delay_Ms(1);
        }
        if (token != 0xfe) goto fail;
        for (int i = 0; i < 512; ++i) *buffer++ = transfer(0xff);
        transfer(0xff); transfer(0xff);
        sector++;
    }
    deselect();
    return RES_OK;
fail:
    deselect();
    return RES_ERROR;
}

DRESULT disk_write(BYTE drive, const BYTE *buffer, LBA_t sector, UINT count)
{
    if (drive || (status & STA_NOINIT) || !count) return RES_NOTRDY;
    while (count--) {
        if (command(24, block_addressing ? sector : sector * 512) != 0) goto fail;
        transfer(0xfe);
        for (int i = 0; i < 512; ++i) transfer(*buffer++);
        transfer(0xff); transfer(0xff);
        if ((transfer(0xff) & 0x1f) != 0x05 || !ready(1000)) goto fail;
        sector++;
    }
    deselect();
    return RES_OK;
fail:
    deselect();
    return RES_ERROR;
}

DRESULT disk_ioctl(BYTE drive, BYTE cmd, void *buffer)
{
    if (drive || (status & STA_NOINIT)) return RES_NOTRDY;
    switch (cmd) {
    case CTRL_SYNC:
        if (!select_card()) return RES_ERROR;
        deselect();
        return RES_OK;
    case GET_SECTOR_SIZE: *(WORD *)buffer = 512; return RES_OK;
    case GET_BLOCK_SIZE: *(DWORD *)buffer = 1; return RES_OK;
    default: return RES_PARERR;
    }
}

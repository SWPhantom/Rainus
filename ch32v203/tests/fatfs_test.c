// Exercise the bundled filesystem against an in-memory FAT16 volume.
#include "../fatfs/ff.h"
#include "../fatfs/diskio.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define SECTORS 70000
static BYTE image[SECTORS * 512];

static void word(unsigned offset, unsigned value) {
    image[offset] = value; image[offset + 1] = value >> 8;
}
static void dword(unsigned offset, unsigned long value) {
    word(offset, value); word(offset + 2, value >> 16);
}
DSTATUS disk_initialize(BYTE p) { return p ? STA_NOINIT : 0; }
DSTATUS disk_status(BYTE p) { return p ? STA_NOINIT : 0; }
DRESULT disk_read(BYTE p, BYTE *b, LBA_t s, UINT n) {
    if (p || !n || s >= SECTORS || n > SECTORS - s) return RES_PARERR;
    memcpy(b, image + s * 512, n * 512); return RES_OK;
}
DRESULT disk_write(BYTE p, const BYTE *b, LBA_t s, UINT n) {
    if (p || !n || s >= SECTORS || n > SECTORS - s) return RES_PARERR;
    memcpy(image + s * 512, b, n * 512); return RES_OK;
}
DRESULT disk_ioctl(BYTE p, BYTE cmd, void *b) {
    if (p) return RES_PARERR;
    if (cmd == CTRL_SYNC) return RES_OK;
    if (cmd == GET_SECTOR_SIZE) { *(WORD *)b = 512; return RES_OK; }
    if (cmd == GET_BLOCK_SIZE) { *(DWORD *)b = 1; return RES_OK; }
    if (cmd == GET_SECTOR_COUNT) { *(LBA_t *)b = SECTORS; return RES_OK; }
    return RES_PARERR;
}

static void make_fat16(void) {
    memset(image, 0, sizeof(image));
    image[0] = 0xeb; image[1] = 0x3c; image[2] = 0x90;
    memcpy(image + 3, "MSDOS5.0", 8);
    word(11, 512); image[13] = 1; word(14, 1); image[16] = 2;
    word(17, 512); word(19, 8192); image[21] = 0xf8; word(22, 32);
    word(24, 32); word(26, 64); image[36] = 0x80; image[38] = 0x29;
    memcpy(image + 43, "RAINUS     ", 11); memcpy(image + 54, "FAT16   ", 8);
    image[510] = 0x55; image[511] = 0xaa;
    for (unsigned sector = 1; sector <= 33; sector += 32) {
        image[sector * 512] = 0xf8; image[sector * 512 + 1] = 0xff;
        image[sector * 512 + 2] = 0xff; image[sector * 512 + 3] = 0xff;
    }
}

static void make_fat32(void) {
    memset(image, 0, sizeof(image));
    image[0] = 0xeb; image[1] = 0x58; image[2] = 0x90;
    memcpy(image + 3, "MSWIN4.1", 8);
    word(11, 512); image[13] = 1; word(14, 32); image[16] = 2;
    image[21] = 0xf8; word(24, 32); word(26, 64);
    dword(32, SECTORS); dword(36, 600); dword(44, 2); word(48, 1); word(50, 6);
    image[64] = 0x80; image[66] = 0x29;
    memcpy(image + 71, "RAINUS     ", 11); memcpy(image + 82, "FAT32   ", 8);
    image[510] = 0x55; image[511] = 0xaa;
    dword(512, 0x41615252); dword(512 + 484, 0x61417272);
    dword(512 + 488, 0xffffffff); dword(512 + 492, 0xffffffff);
    dword(512 + 508, 0xaa550000);
    for (unsigned sector = 32; sector <= 632; sector += 600) {
        dword(sector * 512, 0x0ffffff8);
        dword(sector * 512 + 4, 0x0fffffff);
        dword(sector * 512 + 8, 0x0fffffff); // root directory cluster
    }
}

static void check_volume(void) {
    FATFS fs;
    FIL f;
    UINT n;
    assert(f_mount(&fs, "", 1) == FR_OK);
    assert(f_open(&f, "RAINLOG.TXT", FA_OPEN_APPEND | FA_WRITE) == FR_OK);
    assert(f_write(&f, "first\r\n", 7, &n) == FR_OK && n == 7);
    assert(f_close(&f) == FR_OK);
    assert(f_open(&f, "RAINLOG.TXT", FA_OPEN_APPEND | FA_WRITE) == FR_OK);
    assert(f_write(&f, "second\r\n", 8, &n) == FR_OK && n == 8);
    assert(f_close(&f) == FR_OK);
    assert(f_mount(0, "", 0) == FR_OK);
    assert(f_mount(&fs, "", 1) == FR_OK);
    assert(f_open(&f, "RAINLOG.TXT", FA_READ) == FR_OK);
    char result[16] = {0};
    assert(f_read(&f, result, 15, &n) == FR_OK && n == 15);
    assert(strcmp(result, "first\r\nsecond\r\n") == 0);
    assert(f_close(&f) == FR_OK);
    assert(f_unlink("RAINLOG.TXT") == FR_OK);
    assert(f_open(&f, "RAINLOG.TXT", FA_READ) == FR_NO_FILE);
}

int main(void) {
    make_fat16(); check_volume();
    make_fat32(); check_volume();
    puts("FAT16/FAT32 append, remount, read, and unlink tests passed");
}

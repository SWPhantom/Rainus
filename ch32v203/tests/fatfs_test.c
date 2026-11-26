// Exercise the bundled filesystem against in-memory FAT16 and FAT32 volumes.
#include "../fatfs/ff.h"
#include "../fatfs/diskio.h"
#include "../log.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifndef LOG_BUFFER_LENGTH
#define LOG_BUFFER_LENGTH 10
#endif
#if LOG_BUFFER_LENGTH < 1
#error LOG_BUFFER_LENGTH must be a positive integer.
#endif
#define SECTORS 70000
static BYTE image[SECTORS * 512];
static unsigned read_sectors, write_sectors, sync_calls;
static int fail_sync;
static unsigned writes_to_fail;

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
    read_sectors += n;
    memcpy(b, image + s * 512, n * 512); return RES_OK;
}
DRESULT disk_write(BYTE p, const BYTE *b, LBA_t s, UINT n) {
    if (p || !n || s >= SECTORS || n > SECTORS - s) return RES_PARERR;
    if (writes_to_fail) { --writes_to_fail; return RES_ERROR; }
    write_sectors += n;
    memcpy(image + s * 512, b, n * 512); return RES_OK;
}
DRESULT disk_ioctl(BYTE p, BYTE cmd, void *b) {
    if (p) return RES_PARERR;
    if (cmd == CTRL_SYNC) {
        ++sync_calls;
        return fail_sync ? RES_ERROR : RES_OK;
    }
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
    assert(f_mount(0, "", 0) == FR_OK);
}

static void check_log_format(void) {
    FATFS fs;
    FIL file;
    UINT n;
    char result[256] = {0};
    const char *id = "CDAB207B0FBCFCE239E339E3";
    const uint32_t first[] = {1794274153, 1794274155};
    const uint32_t second[] = {1794274157};
    const char *expected =
        "chip_id,event_unix_utc,build_unix_utc\r\n"
        "CDAB207B0FBCFCE239E339E3,1794274153,1794274141\r\n"
        "CDAB207B0FBCFCE239E339E3,1794274155,1794274141\r\n"
        "CDAB207B0FBCFCE239E339E3,1794274157,1794274200\r\n";
    assert(f_mount(&fs, "", 1) == FR_OK);
    assert(log_close()); // Closing an unused logger does nothing.
    unsigned before_sync = sync_calls;
    assert(log_write_batch(id, first, 2, 1794274141));
    assert(sync_calls == before_sync + 1);
    assert(log_write_batch(id, second, 1, 1794274200));
    assert(sync_calls == before_sync + 2);
    FILINFO info;
    assert(f_stat("RAINLOG.TXT", &info) == FR_OK);
    assert(info.fsize == strlen(expected)); // Size committed before close.
    assert(log_close());
    assert(log_close()); // Explicit cleanup is safe to repeat.
    assert(f_open(&file, "RAINLOG.TXT", FA_READ) == FR_OK);
    assert(f_read(&file, result, sizeof result - 1, &n) == FR_OK);
    assert(n == strlen(expected) && strcmp(result, expected) == 0);
    assert(f_close(&file) == FR_OK);

    // A pre-existing log is appended to, without inserting a header midway.
    assert(f_unlink("RAINLOG.TXT") == FR_OK);
    assert(f_open(&file, "RAINLOG.TXT", FA_CREATE_ALWAYS | FA_WRITE) == FR_OK);
    assert(f_write(&file, "old-format\r\n", 12, &n) == FR_OK && n == 12);
    assert(f_close(&file) == FR_OK);
    assert(log_write_batch(id, second, 1, 1794274200));
    assert(log_close());
    assert(f_open(&file, "RAINLOG.TXT", FA_READ) == FR_OK);
    memset(result, 0, sizeof result);
    assert(f_read(&file, result, sizeof result - 1, &n) == FR_OK);
    assert(strcmp(result, "old-format\r\n"
                  "CDAB207B0FBCFCE239E339E3,1794274157,1794274200\r\n") == 0);
    assert(f_close(&file) == FR_OK);
    assert(f_unlink("RAINLOG.TXT") == FR_OK);
    assert(f_mount(0, "", 0) == FR_OK);
}

static void check_startup_open_and_empty_automatic_batch(void) {
    FATFS fs;
    FILINFO info;
    assert(f_mount(&fs, "", 1) == FR_OK);
    unsigned reads = read_sectors, writes = write_sectors, syncs = sync_calls;
    assert(log_write_batch(NULL, NULL, 0, 1794274141));
    assert(read_sectors == reads && write_sectors == writes && sync_calls == syncs);
    assert(f_stat("RAINLOG.TXT", &info) == FR_NO_FILE);

    assert(log_open());
    assert(f_stat("RAINLOG.TXT", &info) == FR_OK && info.fsize == 0);
    reads = read_sectors; writes = write_sectors; syncs = sync_calls;
    assert(log_open()); // Already open: no second lookup or seek.
    assert(log_write_batch(NULL, NULL, 0, 1794274141));
    assert(read_sectors == reads && write_sectors == writes && sync_calls == syncs);
    assert(log_close());
    assert(f_mount(0, "", 0) == FR_OK);
    assert(f_mount(&fs, "", 1) == FR_OK);
    // Opening at startup creates an empty file, not an early CSV header.
    assert(f_stat("RAINLOG.TXT", &info) == FR_OK && info.fsize == 0);
    assert(f_unlink("RAINLOG.TXT") == FR_OK);
    assert(f_mount(0, "", 0) == FR_OK);
}

static void check_manual_collection_markers(void) {
    FATFS fs;
    FIL reader;
    FILINFO info;
    UINT n;
    const char *id = "CDAB207B0FBCFCE239E339E3";
    uint32_t events[LOG_BUFFER_LENGTH];
    char expected[(3 * LOG_BUFFER_LENGTH + 10) * 64] =
        "chip_id,event_unix_utc,build_unix_utc\r\n";
    char actual[sizeof expected];
    unsigned length = strlen(expected);
    assert(f_mount(&fs, "", 1) == FR_OK);
    assert(log_open()); // A new, empty file prepared during startup.

    // Marker-only first save must create the header and sync once. It must
    // preserve the supplied press time, even without any sensor timestamps.
    unsigned syncs = sync_calls;
    assert(log_write_manual_flush(NULL, NULL, 0, 1794274141, 1794274160));
    assert(sync_calls == syncs + 1);
    const char *first_marker = ",1794274160,\r\n";
    strcpy(expected + length, first_marker);
    length += strlen(first_marker);
    assert(f_stat("RAINLOG.TXT", &info) == FR_OK && info.fsize == length);

    // Partial batches: all their sensor rows precede their marker. For a
    // configured length of 1, the second case intentionally has zero tips.
    const unsigned counts[] = {1, LOG_BUFFER_LENGTH - 1};
    for (unsigned pass = 0; pass < sizeof counts / sizeof counts[0]; ++pass) {
        unsigned count = counts[pass];
        for (unsigned i = 0; i < count; ++i) events[i] = 1794274200 + i;
        syncs = sync_calls;
        assert(log_write_manual_flush(id, events, count, 1794274141, 1794274400));
        assert(sync_calls == syncs + 1); // Not a separate sync for the marker.
        for (unsigned i = 0; i < count; ++i)
            length += snprintf(expected + length, sizeof expected - length,
                               "%s,%lu,1794274141\r\n", id, (unsigned long)events[i]);
        const char *marker = ",1794274400,\r\n";
        strcpy(expected + length, marker);
        length += strlen(marker);
        assert(f_stat("RAINLOG.TXT", &info) == FR_OK && info.fsize == length);
    }

    // A full automatic batch, followed by repeated collection presses with
    // no pending tips. Both presses must leave distinct rows, not disappear.
    for (unsigned i = 0; i < LOG_BUFFER_LENGTH; ++i) events[i] = 1794274500 + i;
    syncs = sync_calls;
    assert(log_write_batch(id, events, LOG_BUFFER_LENGTH, 1794274141));
    assert(sync_calls == syncs + 1);
    for (unsigned i = 0; i < LOG_BUFFER_LENGTH; ++i)
        length += snprintf(expected + length, sizeof expected - length,
                           "%s,%lu,1794274141\r\n", id, (unsigned long)events[i]);
    assert(log_write_manual_flush(NULL, NULL, 0, 1794274141, 1794274700));
    assert(log_write_manual_flush(NULL, NULL, 0, 1794274141, 1794274701));
    assert(sync_calls == syncs + 3);
    const char *last_markers = ",1794274700,\r\n,1794274701,\r\n";
    strcpy(expected + length, last_markers);
    length += strlen(last_markers);

    // Normal logging resumes after collection; no second header and no
    // overwriting earlier rows, including across sector/cluster boundaries.
    const uint32_t next_tip = 1794274800;
    assert(log_write_batch(id, &next_tip, 1, 1794274141));
    assert(sync_calls == syncs + 4);
    length += snprintf(expected + length, sizeof expected - length,
                       "%s,1794274800,1794274141\r\n", id);
    assert(f_stat("RAINLOG.TXT", &info) == FR_OK && info.fsize == length);
    assert(log_close());
    assert(f_open(&reader, "RAINLOG.TXT", FA_READ) == FR_OK);
    assert(f_read(&reader, actual, sizeof actual, &n) == FR_OK && n == length);
    assert(memcmp(actual, expected, length) == 0 && f_eof(&reader));
    assert(f_close(&reader) == FR_OK);
    assert(f_unlink("RAINLOG.TXT") == FR_OK);
    assert(f_mount(0, "", 0) == FR_OK);
}

static void check_manual_flush_failures(void) {
    FATFS fs;
    const char *id = "CDAB207B0FBCFCE239E339E3";
    const uint32_t event = 1794274153;
    assert(f_mount(&fs, "", 1) == FR_OK);
    // Test both marker-only and partial saves. A failed final sync must not
    // be reported as success even if the row writes themselves succeeded.
    for (unsigned count = 0; count <= 1; ++count) {
        assert(log_open());
        unsigned syncs = sync_calls;
        fail_sync = 1;
        assert(!log_write_manual_flush(id, &event, count, 1794274141, 1794274160));
        assert(sync_calls == syncs + 1);
        fail_sync = 0;
        assert(log_close()); // Cleanup only; firmware would stop with error 5.
        assert(f_unlink("RAINLOG.TXT") == FR_OK);
    }
    assert(f_mount(0, "", 0) == FR_OK);
}

static void check_manual_write_failure(void) {
    FATFS fs;
    const char *id = "CDAB207B0FBCFCE239E339E3";
    const uint32_t event = 1794274153;
    assert(f_mount(&fs, "", 1) == FR_OK);
    assert(log_open());
    writes_to_fail = 1;
    // More than a sector's worth of rows forces an actual write before the
    // marker. Its failure must not turn into a successful collection result.
    uint32_t events[20];
    for (unsigned i = 0; i < 20; ++i) events[i] = event + i;
    assert(!log_write_manual_flush(id, events, 20, 1794274141, 1794274200));
    assert(writes_to_fail == 0); // The underlying sector-write failure occurred.
    // The injected failure is over; cleanup does not mean retrying the batch.
    // Firmware stops with error 5 and never acknowledges the failed request.
    assert(log_close());
    assert(f_unlink("RAINLOG.TXT") == FR_OK);
    assert(f_mount(0, "", 0) == FR_OK);
}

static void check_partial_batches(void) {
    FATFS fs;
    FIL reader;
    UINT n;
    const char *id = "CDAB207B0FBCFCE239E339E3";
    const char *header = "chip_id,event_unix_utc,build_unix_utc\r\n";
    const uint32_t start = 1794274153;
    uint32_t events[LOG_BUFFER_LENGTH];
    const unsigned counts[] = {1, LOG_BUFFER_LENGTH - 1, LOG_BUFFER_LENGTH};
    unsigned total = 0;
    assert(f_mount(&fs, "", 1) == FR_OK);
    assert(log_open());
    for (unsigned batch = 0; batch < sizeof counts / sizeof counts[0]; ++batch) {
        unsigned count = counts[batch];
        for (unsigned i = 0; i < count; ++i) events[i] = start + total + i;
        unsigned syncs = sync_calls;
        assert(log_write_batch(id, events, count, 1794274141));
        assert(sync_calls == syncs + (count != 0));
        total += count;
        FILINFO info;
        assert(f_stat("RAINLOG.TXT", &info) == FR_OK);
        assert(info.fsize == strlen(header) + total * 48);
        unsigned reads = read_sectors, writes = write_sectors;
        syncs = sync_calls;
        assert(log_write_batch(NULL, NULL, 0, 1794274141));
        assert(read_sectors == reads && write_sectors == writes && sync_calls == syncs);
    }
    assert(log_close());
    assert(f_open(&reader, "RAINLOG.TXT", FA_READ) == FR_OK);
    char actual[64], expected[64];
    assert(f_read(&reader, actual, strlen(header), &n) == FR_OK && n == strlen(header));
    assert(memcmp(actual, header, n) == 0);
    for (unsigned i = 0; i < total; ++i) {
        int length = snprintf(expected, sizeof expected, "%s,%lu,1794274141\r\n",
                              id, (unsigned long)(start + i));
        assert(length > 0 && (unsigned)length < sizeof expected);
        assert(f_read(&reader, actual, length, &n) == FR_OK && n == (UINT)length);
        assert(memcmp(actual, expected, length) == 0);
    }
    assert(f_eof(&reader));
    assert(f_close(&reader) == FR_OK);
    assert(f_unlink("RAINLOG.TXT") == FR_OK);
    assert(f_mount(0, "", 0) == FR_OK);
}

static void check_retained_cursor(unsigned seed_bytes) {
    FATFS fs;
    FIL seed, reader;
    UINT n;
    const char *id = "CDAB207B0FBCFCE239E339E3";
    const uint32_t start_time = 1794274153;
    uint32_t events[10];
    char padding[512];
    memset(padding, 'x', sizeof padding);
    assert(f_mount(&fs, "", 1) == FR_OK);

    // Seed a contiguous file spanning many clusters on the large-file pass.
    // Only startup open and an explicit reopen should walk that chain.
    assert(f_open(&seed, "RAINLOG.TXT", FA_CREATE_ALWAYS | FA_WRITE) == FR_OK);
    for (unsigned offset = 0; offset < seed_bytes; offset += sizeof padding)
        assert(f_write(&seed, padding, sizeof padding, &n) == FR_OK && n == sizeof padding);
    assert(f_close(&seed) == FR_OK);

    read_sectors = 0;
    assert(log_open());
    unsigned opening_reads = read_sectors;
    unsigned max_batch_reads = 0;
    const unsigned batches = 33;
    for (unsigned batch = 0; batch < batches; ++batch) {
        for (unsigned i = 0; i < 10; ++i) events[i] = start_time + batch * 10 + i;
        read_sectors = 0;
        assert(log_write_batch(id, events, 10, 1794274141));
        if (read_sectors > max_batch_reads) max_batch_reads = read_sectors;
    }
    // Later batches must not repeat the large-file traversal. This bound
    // allows sector/cluster crossings and metadata I/O in these test volumes.
    assert(max_batch_reads <= 12);
    if (seed_bytes >= 1024 * 1024) assert(opening_reads > 2 * max_batch_reads);

    assert(log_close());
    for (unsigned i = 0; i < 10; ++i) events[i] = start_time + batches * 10 + i;
    read_sectors = 0;
    assert(log_open());
    unsigned reopen_reads = read_sectors;
    if (seed_bytes >= 1024 * 1024) assert(reopen_reads > 2 * max_batch_reads);
    read_sectors = 0;
    assert(log_write_batch(id, events, 10, 1794274141));
    assert(read_sectors <= 12);
    assert(log_close());

    // Verify the retained cursor advanced correctly over sector/cluster
    // boundaries, and reopening appended without overwriting earlier rows.
    assert(f_open(&reader, "RAINLOG.TXT", FA_READ) == FR_OK);
    char actual[64], expected[64];
    assert(f_read(&reader, actual, 1, &n) == FR_OK && n == 1 && actual[0] == 'x');
    assert(f_lseek(&reader, seed_bytes - 1) == FR_OK);
    assert(f_read(&reader, actual, 1, &n) == FR_OK && n == 1 && actual[0] == 'x');
    for (unsigned i = 0; i < (batches + 1) * 10; ++i) {
        int length = snprintf(expected, sizeof expected, "%s,%lu,1794274141\r\n",
                              id, (unsigned long)(start_time + i));
        assert(length > 0 && (unsigned)length < sizeof expected);
        assert(f_read(&reader, actual, length, &n) == FR_OK && n == (UINT)length);
        assert(memcmp(actual, expected, length) == 0);
    }
    assert(f_eof(&reader));
    assert(f_close(&reader) == FR_OK);
    assert(f_unlink("RAINLOG.TXT") == FR_OK);
    assert(f_mount(0, "", 0) == FR_OK);
    printf("%u-byte seed: sector reads startup=%u, batch max=%u, reopen=%u\n",
           seed_bytes, opening_reads, max_batch_reads, reopen_reads);
}

static void check_sync_failure(void) {
    FATFS fs;
    const uint32_t event = 1794274153;
    assert(f_mount(&fs, "", 1) == FR_OK);
    fail_sync = 1;
    assert(!log_write_batch("CDAB207B0FBCFCE239E339E3", &event, 1, 1794274141));
    fail_sync = 0;
    // Test cleanup only. Field firmware stops on the failed batch.
    assert(log_close());
    assert(f_unlink("RAINLOG.TXT") == FR_OK);
    assert(f_mount(0, "", 0) == FR_OK);
}

int main(void) {
    make_fat16(); check_volume(); check_log_format();
    check_startup_open_and_empty_automatic_batch(); check_partial_batches();
    check_manual_collection_markers(); check_manual_flush_failures(); check_manual_write_failure();
    check_retained_cursor(512); check_retained_cursor(3 * 1024 * 1024);
    check_sync_failure();
    make_fat32(); check_volume(); check_log_format();
    check_startup_open_and_empty_automatic_batch(); check_partial_batches();
    check_manual_collection_markers(); check_manual_flush_failures(); check_manual_write_failure();
    check_retained_cursor(512); check_retained_cursor(3 * 1024 * 1024);
    check_sync_failure();
    puts("FAT16/FAT32 batch, collection marker, cursor and failure tests passed");
}

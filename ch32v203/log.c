// Turn sensor timestamps and collection-button presses into CSV text.
// Startup finds the file's end once. Each batch adds rows at that position,
// then asks FatFs (the FAT filesystem library) to save its pending changes.
// All functions here run in the main loop, never in an input interrupt.
#include "log.h"
#include "fatfs/ff.h"
#include <stdio.h>

// STOP sleep retains RAM, including FatFs's byte position and current cluster.
// Keep the whole file object; a byte offset alone would still need a seek.
static FIL file;
static int file_open;

int log_open(void)
{
    if (file_open) return 1;
    if (f_open(&file, "RAINLOG.TXT", FA_OPEN_APPEND | FA_WRITE) != FR_OK) return 0;
    file_open = 1;
    return 1;
}

// f_write can report success with fewer bytes written when the card is full.
// Treat either a filesystem error or a short write as failure.
static int write_text(const char *text, unsigned length)
{
    UINT written = 0;
    return f_write(&file, text, length, &written) == FR_OK && written == length;
}

// Shared automatic/manual path. collection_time is NULL for an automatic
// batch; otherwise it points to the Unix time captured at the A2 press.
// Pending sensor rows come first, then the collection marker, then ONE sync.
static int append_batch(const char *chip_id, const uint32_t *events,
                        unsigned count, uint32_t build_time,
                        const uint32_t *collection_time)
{
    static const char header[] = "chip_id,event_unix_utc,build_unix_utc\r\n";
    if (count == 0 && collection_time == NULL) return 1;
    // Startup normally opens the file. Keep this fallback for standalone use.
    if (!log_open()) return 0;

    int ok = 1;
    // Only new or empty files get a header; never rewrite an existing log.
    if (f_size(&file) == 0) ok = write_text(header, sizeof header - 1);

    // One line fits the 24-character chip ID, two 10-digit timestamps,
    // commas, the Windows/Unix-compatible CRLF ending, and a C terminator.
    char line[64];
    for (unsigned i = 0; i < count && ok; ++i) {
        int length = snprintf(line, sizeof line, "%s,%lu,%lu\r\n", chip_id,
                              (unsigned long)events[i], (unsigned long)build_time);
        if (length <= 0 || (unsigned)length >= sizeof line ||
            !write_text(line, length))
            ok = 0;
    }
    if (ok && collection_time != NULL) {
        // Same three columns as a sensor row. Empty ID/build columns identify
        // a collection marker; its middle column is the time of the press.
        int length = snprintf(line, sizeof line, ",%lu,\r\n",
                              (unsigned long)*collection_time);
        if (length <= 0 || (unsigned)length >= sizeof line ||
            !write_text(line, length))
            ok = 0;
    }
    // Flush data and metadata, leaving the cursor available across sleep.
    // As before, the caller stops on failure rather than retrying partial rows.
    if (f_sync(&file) != FR_OK) ok = 0;
    return ok;
}

int log_write_batch(const char *chip_id, const uint32_t *events,
                    unsigned count, uint32_t build_time)
{
    return append_batch(chip_id, events, count, build_time, NULL);
}

int log_write_manual_flush(const char *chip_id, const uint32_t *events,
                           unsigned count, uint32_t build_time,
                           uint32_t collection_time)
{
    return append_batch(chip_id, events, count, build_time, &collection_time);
}

int log_close(void)
{
    if (!file_open) return 1;
    if (f_close(&file) != FR_OK) return 0;
    file_open = 0;
    return 1;
}

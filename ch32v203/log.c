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

int log_write_batch(const char *chip_id, const uint32_t *events,
                    unsigned count, uint32_t build_time)
{
    static const char header[] = "chip_id,event_unix_utc,build_unix_utc\r\n";
    // An empty manual flush needs no card access, even before opening a file.
    if (count == 0) return 1;
    // Startup normally opens the file. Keep this fallback for standalone use.
    if (!log_open()) return 0;

    int ok = 1;
    UINT written = 0;
    // Only new or empty files get a header; never rewrite an existing log.
    if (f_size(&file) == 0 &&
        (f_write(&file, header, sizeof header - 1, &written) != FR_OK ||
         written != sizeof header - 1)) ok = 0;

    for (unsigned i = 0; i < count && ok; ++i) {
        char line[64];
        int length = snprintf(line, sizeof line, "%s,%lu,%lu\r\n", chip_id,
                              (unsigned long)events[i], (unsigned long)build_time);
        written = 0;
        if (length <= 0 || (unsigned)length >= sizeof line ||
            f_write(&file, line, length, &written) != FR_OK || written != (UINT)length)
            ok = 0;
    }
    // Flush data and metadata, leaving the cursor available across sleep.
    // As before, the caller stops on failure rather than retrying partial rows.
    if (f_sync(&file) != FR_OK) ok = 0;
    return ok;
}

int log_close(void)
{
    if (!file_open) return 1;
    if (f_close(&file) != FR_OK) return 0;
    file_open = 0;
    return 1;
}

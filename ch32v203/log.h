#ifndef RAINUS_LOG_H
#define RAINUS_LOG_H
#include <stdint.h>

// Open/create RAINLOG.TXT and find its end after mounting, during startup.
// Idempotent while open. Return 1 on success, 0 on error. Writes no CSV header.
int log_open(void);

// Append and sync each nonempty batch while keeping the file open. Open it
// if necessary. An empty batch succeeds without any card I/O. Call only from
// foreground code after mounting.
// Return 1 on success, 0 on an error. Some rows may already be saved on
// failure; the caller must stop rather than retry the batch.
int log_write_batch(const char *chip_id, const uint32_t *events,
                    unsigned count, uint32_t build_time);

// Explicit cleanup for tests or a controlled shutdown; normal logging keeps
// the file open. Close before remounting or accessing the file via another
// handle. Return 1 on success (also if already closed), 0 on a close failure.
int log_close(void);
#endif

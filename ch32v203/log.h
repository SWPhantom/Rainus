#ifndef RAINUS_LOG_H
#define RAINUS_LOG_H
#include <stdint.h>

// CSV storage API. All timestamps are whole Unix seconds in UTC. FatFs and
// the card must be mounted before use; the caller owns the timestamp arrays.

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

// Save pending sensor rows followed by ",collection_time,\r\n", with a single
// sync for both. Even count=0 writes a marker; events/chip_id may be NULL then.
// collection_time is captured at the button press, not when this function runs.
// Return/error policy is the same as log_write_batch. On failure the caller
// must keep the pending count and stop, without acknowledging or retrying.
int log_write_manual_flush(const char *chip_id, const uint32_t *events,
                           unsigned count, uint32_t build_time,
                           uint32_t collection_time);

// Explicit cleanup for tests or a controlled shutdown; normal logging keeps
// the file open. Close before remounting or accessing the file via another
// handle. Return 1 on success (also if already closed), 0 on a close failure.
int log_close(void);
#endif

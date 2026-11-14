#ifndef RAINUS_LOG_H
#define RAINUS_LOG_H
#include <stdint.h>

// Append CSV rows and close the file. Return 1 on success, 0 on an error.
// On failure, some rows may already have been written; this is not atomic.
int log_write_batch(const char *chip_id, const uint32_t *events,
                    unsigned count, uint32_t build_time);
#endif

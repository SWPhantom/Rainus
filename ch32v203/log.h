#ifndef RAINUS_LOG_H
#define RAINUS_LOG_H
#include <stdint.h>

// Append a complete batch; return zero if the card or filesystem reports an error.
int log_write_batch(const char *chip_id, const uint32_t *events,
                    unsigned count, uint32_t build_time);
#endif

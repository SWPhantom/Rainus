#ifndef RAINUS_RTC_H
#define RAINUS_RTC_H
#include <stdint.h>
typedef enum {
    CLOCK_OK, CLOCK_WRONG_SOURCE, CLOCK_CRYSTAL_TIMEOUT,
    CLOCK_SYNC_TIMEOUT, CLOCK_WRITE_TIMEOUT, CLOCK_MOVED_BACKWARDS
} clock_result_t;
// Start the RTC using the crystal. Set it to build_time if newly enabled or
// earlier than the build; otherwise keep its existing time across a reset.
clock_result_t clock_init(uint32_t build_time);
// Read the RTC's whole Unix seconds (UTC).
uint32_t clock_seconds(void);
// Read seconds plus the divider as 32768 ticks per second. Return 1 on
// success or 0 if the RTC registers do not synchronize after STOP.
int clock_ticks(uint64_t *ticks);
#endif

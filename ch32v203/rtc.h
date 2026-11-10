#ifndef RAINUS_RTC_H
#define RAINUS_RTC_H
#include <stdint.h>
typedef enum {
    CLOCK_OK, CLOCK_WRONG_SOURCE, CLOCK_CRYSTAL_TIMEOUT,
    CLOCK_SYNC_TIMEOUT, CLOCK_WRITE_TIMEOUT
} clock_result_t;
clock_result_t clock_init(uint32_t build_time);
uint32_t clock_seconds(void);
// 32768 ticks per second, used only for debouncing switch edges.
int clock_ticks(uint64_t *ticks);
#endif

#ifndef RAINUS_RTC_H
#define RAINUS_RTC_H
#include <stdint.h>
// Clock API: Unix seconds in UTC, using the Real-Time Clock (RTC) crystal.
// A result other than CLOCK_OK is a terminal fault in the application.
typedef enum {
    CLOCK_OK = 0,
    CLOCK_WRONG_SOURCE,      // RTC source isn't the external 32768 Hz crystal.
    CLOCK_CRYSTAL_TIMEOUT,   // External 32768 Hz crystal did not become ready.
    CLOCK_SYNC_TIMEOUT,      // Bus-side RTC registers did not synchronize.
    CLOCK_WRITE_TIMEOUT,     // RTC did not finish its configuration write.
    CLOCK_MOVED_BACKWARDS    // Timestamp was earlier than the last accepted one.
} clock_result_t;
// The caller stops with LED error 2 on any clock fault. The awake USB debug
// build also reports this enum's numeric value in its rtc= field.
// Start the RTC using the crystal. Set it to build_time if newly enabled or
// earlier than the build; otherwise keep its existing time across a reset.
clock_result_t clock_init(uint32_t build_time);
// Read the RTC's whole Unix seconds (UTC).
uint32_t clock_seconds(void);
// Read Unix seconds plus a fractional second, measured in 32768 Hz ticks.
// Return 1 on success and place the time in *ticks. Return 0 on a sync timeout;
// *ticks is then unchanged and must not be used. The pointer lets us return
// both the timestamp and a success/failure result; zero is a valid timestamp.
int clock_ticks(uint64_t *ticks);
#endif

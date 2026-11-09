#ifndef RAINUS_TIME_POLICY_H
#define RAINUS_TIME_POLICY_H
#include <stdint.h>

typedef enum {
    TIME_UNSET = 0,
    TIME_BUILD = 1,
    TIME_SET = 2
} time_source_t;

// Backup-register values: preserve compatibility with the previous prototype.
#define TIME_MARKER_SET 0x5241u
#define TIME_MARKER_BUILD 0x5242u

time_source_t time_source_from_marker(uint16_t marker);
int time_needs_build_floor(time_source_t source, uint32_t rtc, uint32_t build);
#endif

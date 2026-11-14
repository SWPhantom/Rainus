#ifndef RAINUS_EVENT_H
#define RAINUS_EVENT_H
#include <stdint.h>

// One debounce state for the switch. Initialize to {0} before the first edge.
// last_tick is the time of the last accepted edge, not the last bounce.
typedef struct {
    uint64_t last_tick;
    int seen;
} debounce_t;

typedef enum {
    DEBOUNCE_CLOCK_BACKWARDS = -1, // RTC time is earlier than the last accepted edge.
    DEBOUNCE_IGNORED = 0,         // Less than 100 ms since the last accepted edge.
    DEBOUNCE_ACCEPTED = 1         // First edge, or at least 100 ms since the last.
} debounce_result_t;

// Examine one falling edge, timed in 32768 Hz RTC ticks. Only an accepted
// edge updates state; the caller decides how to handle each result.
debounce_result_t debounce_edge(debounce_t *state, uint64_t tick);
#endif

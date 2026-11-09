#ifndef RAINUS_EVENT_H
#define RAINUS_EVENT_H
#include <stdint.h>

typedef struct {
    uint8_t stable_closed;
    uint8_t candidate_closed;
    uint16_t candidate_ms;
} debounce_t;

// Returns 1 once per stable closure. Call at the nominal 1 ms sampling interval.
int debounce_sample(debounce_t *d, int closed);
void format_timestamp(uint32_t unix_seconds, char out[20]);
#endif

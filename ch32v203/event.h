#ifndef RAINUS_EVENT_H
#define RAINUS_EVENT_H
#include <stdint.h>
typedef struct {
    uint64_t last_tick;
    int seen;
} debounce_t;
// Return 1 to accept, 0 to debounce, or -1 if the clock moved backwards.
int debounce_edge(debounce_t *state, uint64_t tick);
#endif

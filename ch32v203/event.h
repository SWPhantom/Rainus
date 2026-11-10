#ifndef RAINUS_EVENT_H
#define RAINUS_EVENT_H
#include <stdint.h>
typedef struct {
    uint64_t last_tick;
    int seen;
} debounce_t;
// Accept the first falling edge, then ignore edges for 100 ms.
int debounce_edge(debounce_t *state, uint64_t tick);
#endif

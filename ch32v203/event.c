#include "event.h"

int debounce_edge(debounce_t *state, uint64_t tick)
{
    // LSE crystal ticks at 32768 Hz; round 100 ms up to 3277 ticks.
    if (state->seen && tick - state->last_tick < 3277) return 0;
    state->seen = 1;
    state->last_tick = tick;
    return 1;
}

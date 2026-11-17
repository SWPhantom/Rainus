#include "event.h"

debounce_result_t debounce_edge(debounce_t *state, uint64_t tick)
{
    // Time must only move forward during a run. A decreasing reading is a
    // fault, not normal bounce. Without this check unsigned subtraction
    // would mistake it for a huge positive interval. This does not detect
    // resets between boots: the debounce state starts empty each boot.
    if (state->seen && tick < state->last_tick) return DEBOUNCE_CLOCK_BACKWARDS;

    // The RTC has 32768 ticks per second; 100 ms rounds up to 3277 ticks.
    if (state->seen && tick - state->last_tick < 3277) return DEBOUNCE_IGNORED;
    state->seen = 1;
    state->last_tick = tick;
    return DEBOUNCE_ACCEPTED;
}

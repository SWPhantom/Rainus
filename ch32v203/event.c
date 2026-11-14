#include "event.h"

debounce_result_t debounce_edge(debounce_t *state, uint64_t tick)
{
    // An RTC reset or counter wrap could make a later reading earlier than
    // the previous one. Report a clock fault instead of letting unsigned
    // subtraction make that look like a very long interval.
    if (state->seen && tick < state->last_tick) return DEBOUNCE_CLOCK_BACKWARDS;

    // The RTC has 32768 ticks per second; 100 ms rounds up to 3277 ticks.
    if (state->seen && tick - state->last_tick < 3277) return DEBOUNCE_IGNORED;
    state->seen = 1;
    state->last_tick = tick;
    return DEBOUNCE_ACCEPTED;
}

// Contact-bounce filtering and a first-in, first-out (FIFO) input queue.
// This file has no hardware dependencies, so it can also run in host tests.
// See event.h for caller rules about interrupt masking and return values.
#include "event.h"

enum { DEBOUNCE_MINIMUM_TICKS = 3277 }; // 100 ms, rounded up at 32768 ticks/s.

debounce_result_t debounce_edge(debounce_t *state, uint64_t tick)
{
    // Time must only move forward during a run. A decreasing reading is a
    // fault, not normal bounce. Without this check unsigned subtraction
    // would mistake it for a huge positive interval. This does not detect
    // resets between boots: the debounce state starts empty each boot.
    if (state->seen && tick < state->last_tick) return DEBOUNCE_CLOCK_BACKWARDS;

    // The RTC has 32768 ticks per second; 100 ms rounds up to 3277 ticks.
    if (state->seen && tick - state->last_tick < DEBOUNCE_MINIMUM_TICKS)
        return DEBOUNCE_IGNORED;
    state->seen = 1;
    state->last_tick = tick;
    return DEBOUNCE_ACCEPTED;
}

int event_queue_push(event_queue_t *queue, input_event_t event)
{
    // Ring buffer: advancing past the last array slot wraps back to slot 0.
    // Reserve one slot so equal read/write indexes unambiguously mean empty.
    unsigned next = (queue->write_index + 1) % EVENT_QUEUE_SIZE;
    if (next == queue->read_index) {
        queue->overflow = 1;
        return 0;
    }
    // Store the complete event BEFORE publishing the new write index.
    queue->items[queue->write_index] = event;
    queue->write_index = next;
    return 1;
}

int event_queue_pop(event_queue_t *queue, input_event_t *event)
{
    if (queue->read_index == queue->write_index) return 0;
    *event = queue->items[queue->read_index];
    queue->read_index = (queue->read_index + 1) % EVENT_QUEUE_SIZE;
    return 1;
}

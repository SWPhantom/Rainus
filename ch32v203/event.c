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

int event_queue_push(event_queue_t *queue, input_event_t event)
{
    unsigned next = (queue->write_index + 1) % EVENT_QUEUE_SIZE;
    if (next == queue->read_index) {
        queue->overflow = 1;
        return 0;
    }
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

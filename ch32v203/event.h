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

typedef enum { EVENT_TIP, EVENT_FLUSH } event_kind_t;
typedef struct {
    event_kind_t kind;
    uint32_t seconds; // Used only by EVENT_TIP; flush requests aren't log rows.
} input_event_t;

#define EVENT_QUEUE_SIZE 32 // One slot stays empty, so capacity is 31 events.
typedef struct {
    volatile input_event_t items[EVENT_QUEUE_SIZE];
    volatile unsigned read_index, write_index;
    volatile int overflow;
} event_queue_t;

// Initialize to {0}. Push from same-priority input interrupts; pop only with
// those interrupts masked. volatile is not a lock. Each function returns 1 on
// success, 0 on full/empty. Overflow stays set until reset (a terminal fault).
int event_queue_push(event_queue_t *queue, input_event_t event);
int event_queue_pop(event_queue_t *queue, input_event_t *event);
#endif

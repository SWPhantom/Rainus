#include "../event.h"
#include <assert.h>
#include <stdio.h>

static void check_debounce(void)
{
    debounce_t d = {0};
    assert(debounce_edge(&d, 32760) == DEBOUNCE_ACCEPTED);
    assert(debounce_edge(&d, 32770) == DEBOUNCE_IGNORED); // crosses a second
    assert(debounce_edge(&d, 32760 + 3276) == DEBOUNCE_IGNORED);
    assert(debounce_edge(&d, 32760 + 3277) == DEBOUNCE_ACCEPTED);
    assert(debounce_edge(&d, 32760 + 5 * 32768) == DEBOUNCE_ACCEPTED);
    assert(debounce_edge(&d, 32760 + 5 * 32768 - 1) == DEBOUNCE_CLOCK_BACKWARDS);
    assert(debounce_edge(&d, 32760 + 5 * 32768 + 1) == DEBOUNCE_IGNORED);
}

static void check_independent_inputs(void)
{
    debounce_t tip = {0}, flush = {0};
    assert(debounce_edge(&tip, 32768) == DEBOUNCE_ACCEPTED);
    assert(debounce_edge(&flush, 32769) == DEBOUNCE_ACCEPTED);
    assert(debounce_edge(&tip, 32770) == DEBOUNCE_IGNORED);
    assert(debounce_edge(&flush, 32771) == DEBOUNCE_IGNORED);
    assert(debounce_edge(&flush, 32769 + 3277) == DEBOUNCE_ACCEPTED);
}

static void check_queue(void)
{
    event_queue_t q = {0};
    input_event_t event;
    assert(!event_queue_pop(&q, &event));

    // Flush stays behind older tips and ahead of newer ones, including when
    // it arrived while foreground code was occupied with an SD write.
    for (unsigned round = 0; round < 100; ++round) {
        assert(event_queue_push(&q, (input_event_t){EVENT_TIP, round * 2}));
        assert(event_queue_push(&q, (input_event_t){EVENT_FLUSH, 0}));
        assert(event_queue_push(&q, (input_event_t){EVENT_TIP, round * 2 + 1}));
        assert(event_queue_pop(&q, &event));
        assert(event.kind == EVENT_TIP && event.seconds == round * 2);
        assert(event_queue_pop(&q, &event) && event.kind == EVENT_FLUSH);
        assert(event_queue_pop(&q, &event));
        assert(event.kind == EVENT_TIP && event.seconds == round * 2 + 1);
        assert(!event_queue_pop(&q, &event));
        assert(!q.overflow);
    }
    // Fill through wraparound. Overflow must not overwrite a queued item.
    for (unsigned i = 0; i < EVENT_QUEUE_SIZE - 1; ++i)
        assert(event_queue_push(&q, (input_event_t){EVENT_TIP, i}));
    assert(!event_queue_push(&q, (input_event_t){EVENT_FLUSH, 0}));
    assert(q.overflow);
    for (unsigned i = 0; i < EVENT_QUEUE_SIZE - 1; ++i) {
        assert(event_queue_pop(&q, &event));
        assert(event.kind == EVENT_TIP && event.seconds == i);
    }
    assert(!event_queue_pop(&q, &event));
    assert(q.overflow); // Fault remains latched even after the queue is drained.
}

int main(void)
{
    check_debounce();
    check_independent_inputs();
    check_queue();
    puts("independent input debounce and event queue tests passed");
}

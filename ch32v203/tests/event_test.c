#include "../event.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int samples(debounce_t *d, int state, int count)
{
    int events = 0;
    while (count--) events += debounce_sample(d, state);
    return events;
}

static void check_time(uint32_t epoch, const char *expected)
{
    char actual[20];
    format_timestamp(epoch, actual);
    assert(strcmp(actual, expected) == 0);
}

int main(void)
{
    debounce_t d = {0};
    assert(samples(&d, 1, 5) == 0); // closure bounce
    assert(samples(&d, 0, 5) == 0);
    assert(samples(&d, 1, 19) == 0);
    assert(samples(&d, 1, 1) == 1);
    assert(samples(&d, 1, 2000) == 0); // held closed
    assert(samples(&d, 0, 10) == 0); // release bounce
    assert(samples(&d, 1, 3) == 0);
    assert(samples(&d, 0, 20) == 0);
    assert(samples(&d, 1, 20) == 1);

    check_time(0, "1970-01-01T00:00:00");
    check_time(946684800, "2000-01-01T00:00:00");
    check_time(951782400, "2000-02-29T00:00:00");
    check_time(1709164800, "2024-02-29T00:00:00");
    check_time(4294967295U, "2106-02-07T06:28:15");
    puts("event and timestamp tests passed");
}

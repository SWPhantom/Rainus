#include "../event.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    debounce_t d = {0};
    assert(debounce_edge(&d, 32760) == DEBOUNCE_ACCEPTED);
    assert(debounce_edge(&d, 32770) == DEBOUNCE_IGNORED); // crosses a second
    assert(debounce_edge(&d, 32760 + 3276) == DEBOUNCE_IGNORED);
    assert(debounce_edge(&d, 32760 + 3277) == DEBOUNCE_ACCEPTED);
    assert(debounce_edge(&d, 32760 + 5 * 32768) == DEBOUNCE_ACCEPTED);
    assert(debounce_edge(&d, 32760 + 5 * 32768 - 1) == DEBOUNCE_CLOCK_BACKWARDS);
    assert(debounce_edge(&d, 32760 + 5 * 32768 + 1) == DEBOUNCE_IGNORED);
    puts("edge debounce tests passed");
}

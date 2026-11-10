#include "../event.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    debounce_t d = {0};
    assert(debounce_edge(&d, 32760));
    assert(!debounce_edge(&d, 32770)); // bounce crosses a second boundary
    assert(!debounce_edge(&d, 32760 + 3276));
    assert(debounce_edge(&d, 32760 + 3277));
    assert(debounce_edge(&d, 32760 + 5 * 32768));
    puts("edge debounce tests passed");
}

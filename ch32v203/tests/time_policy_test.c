#include "../time_policy.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    assert(time_source_from_marker(0) == TIME_UNSET);
    assert(time_source_from_marker(TIME_MARKER_BUILD) == TIME_BUILD);
    assert(time_source_from_marker(TIME_MARKER_SET) == TIME_SET);
    assert(time_needs_build_floor(TIME_UNSET, 0, 200));
    assert(time_needs_build_floor(TIME_SET, 199, 200));
    assert(time_needs_build_floor(TIME_BUILD, 199, 200));
    assert(!time_needs_build_floor(TIME_SET, 200, 200));
    assert(!time_needs_build_floor(TIME_SET, 201, 200));
    assert(!time_needs_build_floor(TIME_BUILD, 201, 200));
    puts("RTC build-time floor policy tests passed");
}

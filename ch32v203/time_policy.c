#include "time_policy.h"

time_source_t time_source_from_marker(uint16_t marker)
{
    if (marker == TIME_MARKER_SET) return TIME_SET;
    if (marker == TIME_MARKER_BUILD) return TIME_BUILD;
    return TIME_UNSET;
}

int time_needs_build_floor(time_source_t source, uint32_t rtc, uint32_t build)
{
    return source == TIME_UNSET || rtc < build;
}

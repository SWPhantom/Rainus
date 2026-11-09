#ifndef RAINUS_RTC_H
#define RAINUS_RTC_H
#include <stdint.h>
#include "time_policy.h"
extern volatile unsigned rtc_debug_step;
int clock_init(void);
time_source_t clock_source(void);
uint32_t clock_seconds(void);
int clock_set(uint32_t seconds, time_source_t source);
#endif

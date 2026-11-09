#ifndef RAINUS_RTC_H
#define RAINUS_RTC_H
#include <stdint.h>
extern volatile unsigned rtc_debug_step;
int clock_init(void);
int clock_is_set(void);
uint32_t clock_seconds(void);
int clock_set(uint32_t seconds);
#endif

#include "event.h"

// Initial conservative setting for bench testing, not a measured gauge spec.
#define DEBOUNCE_MS 20

int debounce_sample(debounce_t *d, int closed)
{
    closed = !!closed;
    if (closed != d->candidate_closed) {
        d->candidate_closed = closed;
        d->candidate_ms = 1;
    } else if (d->candidate_ms < DEBOUNCE_MS) {
        d->candidate_ms++;
    }
    if (d->candidate_ms == DEBOUNCE_MS && d->stable_closed != closed) {
        d->stable_closed = closed;
        return closed;
    }
    return 0;
}

static int leap(unsigned year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static void two(char *p, unsigned n)
{
    p[0] = '0' + n / 10;
    p[1] = '0' + n % 10;
}

void format_timestamp(uint32_t seconds, char out[20])
{
    static const uint8_t lengths[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    uint32_t days = seconds / 86400;
    unsigned rem = seconds % 86400, year = 1970, month = 0;
    while (days >= (uint32_t)(365 + leap(year))) days -= 365 + leap(year++);
    while (days >= (uint32_t)(lengths[month] + (month == 1 && leap(year)))) {
        days -= lengths[month] + (month == 1 && leap(year));
        month++;
    }
    two(out, year / 100); two(out + 2, year % 100); out[4] = '-';
    two(out + 5, month + 1); out[7] = '-'; two(out + 8, days + 1);
    out[10] = 'T'; two(out + 11, rem / 3600); out[13] = ':';
    two(out + 14, rem / 60 % 60); out[16] = ':'; two(out + 17, rem % 60);
    out[19] = '\0';
}

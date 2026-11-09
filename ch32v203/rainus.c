#include "ch32fun.h"
#include "fatfs/ff.h"
#include "rtc.h"
#include "event.h"
#include "usbd.h"
#include <stdio.h>
#include <string.h>

#ifndef RAINUS_BUILD_UNIX_TIME
#error Build with the Makefile so a UTC build time is provided.
#endif
#if RAINUS_BUILD_UNIX_TIME < 1577836800UL || RAINUS_BUILD_UNIX_TIME > 4294967295UL
#error Build time must be a valid Unix timestamp between 2020 and 2106.
#endif

// Give each bench unit a distinct short identifier before building.
#ifndef RAINUS_ID
#define RAINUS_ID "BENCH"
#endif

static FATFS fs;
static FIL file;
extern volatile unsigned sd_debug_step, sd_debug_response;
static unsigned error_code, mount_result, events_written;
static volatile unsigned status_requested;

static const char *source_name(time_source_t source)
{
    return source == TIME_SET ? "SET" : source == TIME_BUILD ? "BUILD" : "UNSET";
}

void HandleUSBInput(int numbytes, uint8_t *data)
{
    for (int i = 0; i < numbytes; ++i)
        if (data[i] == '?' || data[i] == 's' || data[i] == 'S') status_requested = 1;
}

static void debug_status(void)
{
    char text[64];
    if (!USBDCTX.USBD_DevEnumStatus) return;
    int n = snprintf(text, sizeof(text), "err=%u rtc=%u fat=%u sd=%u/%u logs=%u time=%s\r\n",
                     error_code, rtc_debug_step, mount_result, sd_debug_step,
                     sd_debug_response, events_written, source_name(clock_source()));
    if (n > 0 && (unsigned)n < sizeof(text))
        USBD_SendEndpointNEW(3, (uint8_t *)text, n, 1);
}

static void led(int on) { funDigitalWrite(PA15, on ? FUN_LOW : FUN_HIGH); }

// Distinct fatal indication: 1 pulse = RTC; 2 = SD; 3 = time file; 4 = log write.
static void fail(unsigned pulses)
{
    error_code = pulses;
    for (;;) {
        for (unsigned i = 0; i < pulses; ++i) {
            led(1); Delay_Ms(150); led(0); Delay_Ms(200);
        }
        poll_input();
        status_requested = 0;
        debug_status();
        Delay_Ms(1200);
    }
}

// Optional one-shot file, containing a decimal Unix UTC timestamp and newline.
// Delete it before setting the clock so a future reboot cannot silently reuse it.
static int consume_time_file(void)
{
    char text[24];
    UINT n;
    FRESULT r = f_open(&file, "SETTIME.TXT", FA_READ);
    if (r == FR_NO_FILE) return 1;
    if (r != FR_OK) return 0;
    r = f_read(&file, text, sizeof(text) - 1, &n);
    if (f_close(&file) != FR_OK || r != FR_OK || n == 0 || n >= sizeof(text) - 1) return 0;
    text[n] = 0;
    uint32_t timestamp = 0;
    unsigned i = 0;
    while (i < n && text[i] >= '0' && text[i] <= '9') {
        unsigned digit = text[i++] - '0';
        if (timestamp > (UINT32_MAX - digit) / 10) return 0;
        timestamp = timestamp * 10 + digit;
    }
    // An explicit setting may move time backwards, but not below this build's floor.
    if (!i || timestamp < RAINUS_BUILD_UNIX_TIME) return 0;
    while (i < n && (text[i] == '\r' || text[i] == '\n' || text[i] == ' ')) i++;
    if (i != n || f_unlink("SETTIME.TXT") != FR_OK) return 0;
    return clock_set(timestamp, TIME_SET);
}

static int write_event(uint32_t seconds, time_source_t source)
{
    char line[120], iso[20];
    if (source != TIME_UNSET) format_timestamp(seconds, iso);
    else memcpy(iso, "UNSET", 6);
    // The first four columns match the earlier prototype; the final two show
    // clock provenance and the build floor so stale estimates can be identified.
    int len = snprintf(line, sizeof(line), "%s,%s,%lu,%lu,%s,%lu\r\n", RAINUS_ID, iso,
                       (unsigned long)(source != TIME_UNSET ? seconds : 0),
                       (unsigned long)(source != TIME_UNSET && seconds >= 946684800UL ? seconds - 946684800UL : 0),
                       source_name(source), (unsigned long)RAINUS_BUILD_UNIX_TIME);
    if (len <= 0 || (unsigned)len >= sizeof(line)) return 0;
    if (f_open(&file, "RAINLOG.TXT", FA_OPEN_APPEND | FA_WRITE) != FR_OK) return 0;
    UINT written = 0;
    FRESULT r = f_write(&file, line, len, &written);
    FRESULT close_result = f_close(&file);
    return r == FR_OK && written == (UINT)len && close_result == FR_OK;
}

int main(void)
{
    SystemInit();
    funGpioInitAll();
    funDigitalWrite(PA15, FUN_HIGH);
    funPinMode(PA15, GPIO_CFGLR_OUT_10Mhz_PP);
    USBDSetup();
    // Connect the isolated reed switch between PA0 and GND.
    funPinMode(PA0, GPIO_CFGLR_IN_PUPD);
    funDigitalWrite(PA0, FUN_HIGH);
    if (!clock_init()) fail(1);
    if (time_needs_build_floor(clock_source(), clock_seconds(), RAINUS_BUILD_UNIX_TIME) &&
        !clock_set(RAINUS_BUILD_UNIX_TIME, TIME_BUILD)) fail(1);
    mount_result = f_mount(&fs, "", 1);
    if (mount_result != FR_OK) fail(2);
    if (!consume_time_file()) fail(3);

    debounce_t debounce = {0};
    uint32_t edge_time = 0;
    time_source_t edge_source = TIME_UNSET;
    unsigned led_tick = 0, status_tick = 0;
    for (;;) {
        poll_input();
        if (status_requested || ++status_tick >= 2000) {
            debug_status();
            status_requested = 0;
            status_tick = 0;
        }
        int closed = !funDigitalRead(PA0);
        if (closed && !debounce.candidate_closed) {
            edge_time = clock_seconds();
            edge_source = clock_source();
        }
        if (debounce_sample(&debounce, closed)) {
            if (!write_event(edge_time, edge_source)) fail(4);
            events_written++;
            status_requested = 1;
            led_tick = 360;
        }
        // Two 120 ms flashes, with a 120 ms gap. Non-blocking so the
        // indicator does not prevent sampling another gauge closure.
        if (led_tick) {
            led(led_tick > 240 || led_tick <= 120);
            --led_tick;
            if (!led_tick) led(0);
        }
        Delay_Ms(1);
    }
}

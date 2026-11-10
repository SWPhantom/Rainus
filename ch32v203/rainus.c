#include "ch32fun.h"
#include "fatfs/ff.h"
#include "rtc.h"
#include "event.h"
#include "chip_id.h"
#include <stdio.h>
#if RAINUS_DEBUG_USB
#include "usbd.h"
#endif

#ifndef RAINUS_BUILD_UNIX_TIME
#error Build with the Makefile to provide a UTC build time.
#endif
#define BATCH_SIZE 10
#define QUEUE_SIZE 32 // Holds up to 31 edges while foreground code is busy.

static FATFS filesystem;
static char chip_id[CHIP_ID_TEXT_LEN + 1];
static volatile uint32_t queue[QUEUE_SIZE];
static volatile unsigned queue_head, queue_tail, queue_overflow, clock_fault;
static debounce_t debounce;
static uint32_t batch[BATCH_SIZE];
static unsigned batch_count;
#if RAINUS_DEBUG_USB
static volatile int status_requested;
static unsigned saved_count;
static unsigned fatal_error;
static unsigned clock_error;
static volatile unsigned interrupt_count;
void HandleUSBInput(int count, uint8_t *data)
{
    for (int i = 0; i < count; ++i)
        if (data[i] == '?' || data[i] == 's' || data[i] == 'i') status_requested = 1;
}
static void debug_status(void)
{
    if (!status_requested || !USBDCTX.USBD_DevEnumStatus) return;
    char line[150];
    int n = snprintf(line, sizeof line, "id=%s error=%u rtc=%u irq=%u pin=%d saved=%u buffered=%u time=%lu\r\n",
                     chip_id, fatal_error, clock_error, interrupt_count, funDigitalRead(PA0),
                     saved_count, batch_count, (unsigned long)clock_seconds());
    if (n > 0 && (unsigned)n < sizeof line &&
        USBD_SendEndpointNEW(3, (uint8_t *)line, n, 1) == 0) status_requested = 0;
}
#endif

static void led(int on) { funDigitalWrite(PA15, on ? FUN_LOW : FUN_HIGH); }

// 1 = RTC, 2 = SD mount, 3 = queue overflow, 4 = SD write.
static void fail(unsigned pulses)
{
#if RAINUS_DEBUG_USB
    fatal_error = pulses;
#endif
    for (;;) {
        for (unsigned i = 0; i < pulses; ++i) {
            led(1); Delay_Ms(150); led(0); Delay_Ms(200);
        }
#if RAINUS_DEBUG_USB
        poll_input();
        debug_status();
#endif
        Delay_Ms(1200);
    }
}

// This interrupt also runs during SD writes. It captures the time and queues
// it; the slower filesystem and LED operations happen in the main loop.
__attribute__((interrupt)) void EXTI0_IRQHandler(void)
{
    if (!(EXTI->INTFR & EXTI_Line0)) return;
    EXTI->INTFR = EXTI_Line0;
#if RAINUS_DEBUG_USB
    ++interrupt_count;
#endif
    uint64_t ticks;
    if (!clock_ticks(&ticks)) {
        clock_fault = 1;
        return;
    }
    if (!debounce_edge(&debounce, ticks)) return;
    unsigned next = (queue_head + 1) % QUEUE_SIZE;
    if (next == queue_tail) {
        queue_overflow = 1;
        return;
    }
    queue[queue_head] = ticks >> 15;
    queue_head = next;
}

static void enable_switch_interrupt(void)
{
    // A dry contact from PA0 to GND pulls the normally-high pin LOW.
    funPinMode(PA0, GPIO_CFGLR_IN_PUPD);
    funDigitalWrite(PA0, FUN_HIGH);
    AFIO->EXTICR[0] &= ~AFIO_EXTICR1_EXTI0;
    EXTI->RTENR &= ~EXTI_Line0;
    EXTI->FTENR |= EXTI_Line0;
    EXTI->INTFR = EXTI_Line0;
    EXTI->INTENR |= EXTI_Line0;
    // Also latch an event for WFE, even while interrupts are masked.
    EXTI->EVENR |= EXTI_Line0;
    NVIC_EnableIRQ(EXTI0_IRQn);
}

static int write_batch(void)
{
    FIL file;
    if (f_open(&file, "RAINLOG.TXT", FA_OPEN_APPEND | FA_WRITE) != FR_OK) return 0;
    int ok = 1;
    for (unsigned i = 0; i < BATCH_SIZE && ok; ++i) {
        char line[48];
        int length = snprintf(line, sizeof line, "%s,%lu\r\n", chip_id,
                              (unsigned long)batch[i]);
        UINT written = 0;
        if (length <= 0 || (unsigned)length >= sizeof line ||
            f_write(&file, line, length, &written) != FR_OK || written != (UINT)length)
            ok = 0;
    }
    if (f_close(&file) != FR_OK) ok = 0;
    return ok;
}

int main(void)
{
    SystemInit();
    funGpioInitAll();
    // The factory ID occupies 12 consecutive bytes, not a generated ID.
    const volatile uint8_t *uid = (const volatile uint8_t *)&ESIG->UID0;
    uint8_t bytes[CHIP_ID_BYTES];
    for (unsigned i = 0; i < CHIP_ID_BYTES; ++i) bytes[i] = uid[i];
    chip_id_format(bytes, chip_id);
    funDigitalWrite(PA15, FUN_HIGH);
    funPinMode(PA15, GPIO_CFGLR_OUT_10Mhz_PP);
#if RAINUS_DEBUG_USB
    USBDSetup();
#endif
    clock_result_t result = clock_init(RAINUS_BUILD_UNIX_TIME);
    if (result != CLOCK_OK) {
#if RAINUS_DEBUG_USB
        clock_error = result;
#endif
        fail(1);
    }
    enable_switch_interrupt();
    if (f_mount(&filesystem, "", 1) != FR_OK) fail(2);

    for (;;) {
#if RAINUS_DEBUG_USB
        poll_input();
        debug_status();
#endif
        // Keep interrupts masked between the empty check and sleep. EXTI also
        // latches a hardware event: even an edge just before WFE wakes it.
        // Re-enable interrupts after wake to run the queued interrupt.
        __disable_irq();
        __asm__ volatile ("" ::: "memory");
        if (clock_fault || queue_overflow) {
            unsigned error = clock_fault ? 1 : 3;
#if RAINUS_DEBUG_USB
            if (clock_fault) clock_error = CLOCK_SYNC_TIMEOUT;
#endif
            __enable_irq();
            fail(error);
        }
        if (queue_head == queue_tail) {
#if !RAINUS_DEBUG_USB
            PWR->CTLR &= ~(PWR_CTLR_PDDS | PWR_CTLR_LPDS);
            // STOP, not standby: retain RAM. WFITOWFE makes WFI consume
            // a pending EXTI event instead of relying on global IRQ state.
            NVIC->SCTLR |= (1 << 2) | (1 << 3);
            __asm__ volatile ("wfi" ::: "memory");
            NVIC->SCTLR &= ~((1 << 2) | (1 << 3));
#endif
            __asm__ volatile ("" ::: "memory");
            __enable_irq();
            continue;
        }
        uint32_t seconds = queue[queue_tail];
        queue_tail = (queue_tail + 1) % QUEUE_SIZE;
        __asm__ volatile ("" ::: "memory");
        __enable_irq();
        batch[batch_count++] = seconds;
        // A brief blocking indication; interrupts remain enabled during it.
        led(1); Delay_Ms(100); led(0);
        if (batch_count == BATCH_SIZE) {
            if (!write_batch()) fail(4);
            batch_count = 0;
#if RAINUS_DEBUG_USB
            saved_count += BATCH_SIZE;
#endif
        }
    }
}

// Rainus application: sleep -> capture a switch closure -> batch -> save CSV.
//
// Startup runs once: configure the chip, read its factory ID, start the RTC
// (Real-Time Clock), then enable inputs and prepare the SD file. Afterward:
//   A0 interrupt: timestamp/debounce a rain tip and put it in the input queue.
//   A2 interrupt: timestamp/debounce a collection request in that SAME queue.
//   Main loop: take inputs in order; save full batches or a manual collection.
//   No work: enter STOP sleep (RAM retained), except in the USB debug build.
// The interrupt handlers do not write to SD. They can capture new inputs
// while the main loop waits for an earlier SD operation to finish.
//
// Hardware notation used below: a register is a memory address whose reads/
// writes control the chip. REG |= FLAG sets a bit; REG &= ~FLAG clears it.
// The names such as EXTI->INTFR come from ch32fun/the chip manual. They are
// kept to match those sources; comments explain their job at each use.
#include "ch32fun.h"
#include "fatfs/ff.h"
#include "rtc.h"
#include "event.h"
#include "chip_id.h"
#include "log.h"
#include <stdio.h>
#if RAINUS_DEBUG_USB
#include "usbd.h"
#endif

// Set to 0 to disable all firmware-controlled LED flashes for field use.
#define RAINUS_LED_ENABLED 1

#ifndef RAINUS_BUILD_UNIX_TIME
#error Build with the Makefile to provide a UTC build time.
#endif
#if RAINUS_BUILD_UNIX_TIME < 1577836800UL || RAINUS_BUILD_UNIX_TIME > 4294967295UL
#error Build time must be a valid Unix timestamp between 2020 and 2106.
#endif
#ifndef LOG_BUFFER_LENGTH
#define LOG_BUFFER_LENGTH 10
#endif
#if LOG_BUFFER_LENGTH < 1
#error LOG_BUFFER_LENGTH must be a positive integer.
#endif
#define INPUT_LINES (EXTI_Line0 | EXTI_Line2)

// PFIC = Programmable Fast Interrupt Controller (called NVIC by ch32fun).
// These bits in its system-control register choose how a wait instruction
// behaves. Their positions are fixed by this chip, not application options.
#define INTERRUPT_CONTROLLER_DEEP_SLEEP (1u << 2)
#define WAIT_INSTRUCTION_USES_EVENTS    (1u << 3)
#define SET_SOFTWARE_WAKE_EVENT         (1u << 5)

// These numbers are also the repeating LED error codes and USB error values.
typedef enum {
    ERROR_RTC = 2,
    ERROR_SD_MOUNT = 3,
    ERROR_QUEUE_FULL = 4,
    ERROR_SD_WRITE = 5
} logger_error_t;

static FATFS filesystem;
static char chip_id[CHIP_ID_TEXT_LEN + 1];
// Tips and flush markers share one queue, preserving their processing order.
// Both input interrupts have the same priority and cannot preempt each other.
// Foreground code masks interrupts while examining/removing queued items.
static event_queue_t event_queue;
static volatile clock_result_t clock_fault;
static debounce_t tip_debounce, flush_debounce;
// The batch serves a different purpose from the queue: group timestamps into
// one SD operation to save energy. Its contents stay fixed during that write,
// while the interrupt can still add newly arriving events to event_queue.
// All this RAM is lost on reset or power loss. Static variables start at zero.
static uint32_t batch[LOG_BUFFER_LENGTH];
static unsigned batch_count;
#if RAINUS_DEBUG_USB
static volatile int status_requested;
static unsigned saved_count;
static unsigned fatal_error;
static unsigned clock_error;
static volatile unsigned interrupt_count;
// Required callback name from ch32fun's USB library. The computer sends
// single-character commands; defer the status reply to the main loop.
void HandleUSBInput(int count, uint8_t *data)
{
    for (int i = 0; i < count; ++i)
        if (data[i] == '?' || data[i] == 's' || data[i] == 'i') status_requested = 1;
}
static void debug_status(void)
{
    // Enumeration means the computer has recognized/configured the USB device.
    if (!status_requested || !USBDCTX.USBD_DevEnumStatus) return;
    char line[150];
    int n = snprintf(line, sizeof line, "id=%s error=%u rtc=%u irq=%u pin=%d saved=%u buffered=%u time=%lu\r\n",
                     chip_id, fatal_error, clock_error, interrupt_count, funDigitalRead(PA0),
                     saved_count, batch_count, (unsigned long)clock_seconds());
    // Endpoint 3 is the USB channel carrying board -> computer serial text.
    // The library returns zero if it accepted the reply; otherwise try later.
    if (n > 0 && (unsigned)n < sizeof line &&
        USBD_SendEndpointNEW(3, (uint8_t *)line, n, 1) == 0) status_requested = 0;
}
#endif

// Optional USB work runs synchronously in the foreground. This is a no-op
// in the field build; the debug build deliberately stays awake for USB.
static void service_debug(void)
{
#if RAINUS_DEBUG_USB
    poll_input();
    debug_status();
#endif
}

#if RAINUS_LED_ENABLED
static void led(int on) { funDigitalWrite(PA15, on ? FUN_LOW : FUN_HIGH); }
#endif

// Only an explicit manual flush gets a success indication. Interrupts stay
// enabled during these short delays, so new tips continue entering the queue.
static void acknowledge_flush(void)
{
#if RAINUS_LED_ENABLED
    led(1); Delay_Ms(75); led(0); Delay_Ms(75);
    led(1); Delay_Ms(75); led(0);
#endif
}

// Terminal error policy: stop accepting switch events and repeat the error
// code until reset. LED delays block here; USB still runs between groups.
// We do not write a sensor record with an untrusted RTC or retry a partial
// SD batch (which could duplicate rows that were already saved).
static void stop_with_error(logger_error_t error)
{
    NVIC_DisableIRQ(EXTI0_IRQn);
    NVIC_DisableIRQ(EXTI2_IRQn);
#if RAINUS_DEBUG_USB
    fatal_error = error;
#endif
    (void)error;
    for (;;) {
#if RAINUS_LED_ENABLED
        for (unsigned i = 0; i < (unsigned)error; ++i) {
            led(1); Delay_Ms(150); led(0); Delay_Ms(200);
        }
#endif
        service_debug();
        Delay_Ms(1200);
    }
}

// Shared capture path, called only from the two input interrupt handlers.
// Each pin has its own debounce history, so the service
// button cannot debounce away a nearby rain tip (or vice versa). No SD I/O
// happens here. Same-priority handlers serialize access to the RTC and queue.
static void capture_edge(event_kind_t kind, debounce_t *debounce)
{
    uint64_t ticks;
    if (!clock_ticks(&ticks)) {
        clock_fault = CLOCK_SYNC_TIMEOUT;
        return;
    }
    debounce_result_t result = debounce_edge(debounce, ticks);
    if (result == DEBOUNCE_CLOCK_BACKWARDS) {
        clock_fault = CLOCK_MOVED_BACKWARDS;
        return;
    }
    if (result == DEBOUNCE_IGNORED) return;
    // Divide 32768-Hz ticks by 32768 (2^15) for whole Unix seconds.
    input_event_t event = {kind, (uint32_t)(ticks >> 15)};
    event_queue_push(&event_queue, event); // A full queue latches the fault.
}

// EXTI = External Interrupt: chip hardware notices pin voltage changes.
// These function names/interrupt attributes are required by the startup code.
// INTFR holds pending input flags; writing a 1 CLEARS that pending flag (this
// register has special hardware behavior, unlike an ordinary C variable).
// These handlers also run during startup SD preparation and batch writes.
__attribute__((interrupt)) void EXTI0_IRQHandler(void)
{
    if (!(EXTI->INTFR & EXTI_Line0)) return;
    EXTI->INTFR = EXTI_Line0;
#if RAINUS_DEBUG_USB
    ++interrupt_count;
#endif
    capture_edge(EVENT_TIP, &tip_debounce);
}

__attribute__((interrupt)) void EXTI2_IRQHandler(void)
{
    if (!(EXTI->INTFR & EXTI_Line2)) return;
    EXTI->INTFR = EXTI_Line2;
    capture_edge(EVENT_FLUSH, &flush_debounce);
}

static void enable_switch_interrupts(void)
{
    // A dry contact supplies no voltage: it only connects/disconnects wires.
    // Internal pull-up resistors keep PA0/PA2 HIGH while open; connecting to
    // ground makes them LOW. We capture that HIGH -> LOW (falling) transition.
    funPinMode(PA0, GPIO_CFGLR_IN_PUPD);
    funDigitalWrite(PA0, FUN_HIGH);
    funPinMode(PA2, GPIO_CFGLR_IN_PUPD);
    funDigitalWrite(PA2, FUN_HIGH);
    // AFIO (Alternate Function I/O) maps pin inputs to interrupt lines.
    // Clearing these fields selects port A for lines 0 and 2.
    AFIO->EXTICR[0] &= ~(AFIO_EXTICR1_EXTI0 | AFIO_EXTICR1_EXTI2);
    EXTI->RTENR &= ~INPUT_LINES; // Disable rising-edge detection (release).
    EXTI->FTENR |= INPUT_LINES;  // Enable falling-edge detection (closure).
    EXTI->INTFR = INPUT_LINES;
    EXTI->INTENR |= INPUT_LINES; // Let a closure request an interrupt handler.
    // Also remember a hardware event for WFE (Wait For Event) sleep. This
    // wakes the chip even when interrupt handlers are temporarily disabled.
    EXTI->EVENR |= INPUT_LINES;
    // Equal priority prevents the handlers interrupting each other while
    // changing the shared queue or reading the clock. Zero is the priority.
    NVIC_SetPriority(EXTI0_IRQn, 0);
    NVIC_SetPriority(EXTI2_IRQn, 0);
    NVIC_EnableIRQ(EXTI0_IRQn);
    NVIC_EnableIRQ(EXTI2_IRQn);
}

// A compiler barrier emits no CPU instruction. It keeps memory accesses on
// their intended side of interrupt masking; ch32fun's irq helpers do not
// provide that guarantee themselves. This is unrelated to waiting or sleep.
static inline void compiler_barrier(void)
{
    __asm__ volatile ("" ::: "memory");
}

#if !RAINUS_DEBUG_USB
// Called only with interrupts masked and an empty software queue.
// WFE = Wait For Event. EXTI latches either input as a hardware event even with
// interrupts masked, so an edge between the empty check and sleep can wake us.
// WFI = Wait For Interrupt; WFE = Wait For Event. An event here is a hardware
// wake request, not the input_event_t object we later put in our RAM queue.
// This QingKe core uses the RISC-V `wfi` instruction for BOTH WFI and WFE;
// WAIT_INSTRUCTION_USES_EVENTS chooses event behavior. The deep-sleep bit
// enables the deeper wait state; PWR->CTLR selects STOP rather than standby.
// STOP retains our RAM.
static void sleep_until_switch_event(void)
{
    // PWR is the Power Control peripheral. Clear PDDS (selects standby) and
    // LPDS (low-power regulator option): use STOP with the normal regulator.
    PWR->CTLR &= ~(PWR_CTLR_PDDS | PWR_CTLR_LPDS);

    // Set and consume a software event first. An old, uncleared event would
    // otherwise make sleep return immediately and waste power. This sequence
    // is the one that reduced idle current in our hardware comparisons.
    NVIC->SCTLR |= INTERRUPT_CONTROLLER_DEEP_SLEEP | WAIT_INSTRUCTION_USES_EVENTS |
                   SET_SOFTWARE_WAKE_EVENT;
    NVIC->SCTLR &= ~SET_SOFTWARE_WAKE_EVENT;
    __asm__ volatile ("wfi" ::: "memory");

    // If a switch edge arrived during that first wait, let the interrupt run
    // now. Otherwise actually sleep; any edge after this check wakes us.
    // Keep this check instead of ch32fun's __WFE(), which always waits twice.
    if (!(EXTI->INTFR & INPUT_LINES))
        __asm__ volatile ("wfi" ::: "memory");
    NVIC->SCTLR &= ~(INTERRUPT_CONTROLLER_DEEP_SLEEP | WAIT_INSTRUCTION_USES_EVENTS);
}
#endif

// Block until one accepted tip or flush request is available, or stop on a fault.
// In the field build an empty queue leads to STOP, not periodic pin polling.
// A wake can occur without a queued timestamp (e.g. a filtered bounce), so
// check again after waking. Return with interrupts enabled before any SD work.
static input_event_t wait_for_event(void)
{
    for (;;) {
        service_debug();
        __disable_irq();
        compiler_barrier();

        if (clock_fault || event_queue.overflow) {
            logger_error_t error = clock_fault ? ERROR_RTC : ERROR_QUEUE_FULL;
#if RAINUS_DEBUG_USB
            if (clock_fault) clock_error = clock_fault;
#endif
            compiler_barrier();
            __enable_irq();
            stop_with_error(error);
        }
        input_event_t event;
        if (event_queue_pop(&event_queue, &event)) {
            compiler_barrier();
            __enable_irq();
            return event;
        }

#if !RAINUS_DEBUG_USB
        sleep_until_switch_event();
#endif
        compiler_barrier();
        __enable_irq(); // Now run any interrupt that woke us and queued an event.
    }
}

int main(void)
{
    SystemInit();
    funGpioInitAll();
    // ESIG is the chip's Electronic Signature hardware; UID is its factory
    // Unique ID. Read 12 consecutive bytes without writing/assigning an ID.
    const volatile uint8_t *uid = (const volatile uint8_t *)&ESIG->UID0;
    uint8_t bytes[CHIP_ID_BYTES];
    for (unsigned i = 0; i < CHIP_ID_BYTES; ++i) bytes[i] = uid[i];
    chip_id_format(bytes, chip_id);
    funDigitalWrite(PA15, FUN_HIGH); // Blue LED is active LOW; start it off.
    funPinMode(PA15, GPIO_CFGLR_OUT_10Mhz_PP);
#if RAINUS_DEBUG_USB
    USBDSetup();
#endif
    // The RTC must initialize successfully before switch capture is enabled.
    clock_result_t result = clock_init(RAINUS_BUILD_UNIX_TIME);
    if (result != CLOCK_OK) {
#if RAINUS_DEBUG_USB
        clock_error = result;
#endif
        stop_with_error(ERROR_RTC);
    }
    // Capture starts after the RTC is ready. Inputs can queue events while SD
    // startup is busy; the main loop processes them after log_open() finishes.
    enable_switch_interrupts();
    if (f_mount(&filesystem, "", 1) != FR_OK) stop_with_error(ERROR_SD_MOUNT);
    // Pay the initial directory lookup / end-of-file traversal here, rather
    // than on the first data batch. Opening an empty file writes no CSV rows.
    if (!log_open()) stop_with_error(ERROR_SD_WRITE);

    // Initialization above runs once per reset. This loop runs for the whole
    // deployment: wait/asleep -> collect a tip or service a flush request.
    // SD writes block this loop, but both input interrupts stay enabled.
    for (;;) {
        input_event_t event = wait_for_event();
        if (event.kind == EVENT_TIP) batch[batch_count++] = event.seconds;
        if (batch_count == LOG_BUFFER_LENGTH || event.kind == EVENT_FLUSH) {
            unsigned count = batch_count;
            int saved;
            if (event.kind == EVENT_FLUSH)
                saved = log_write_manual_flush(chip_id, batch, count,
                                              RAINUS_BUILD_UNIX_TIME, event.seconds);
            else
                saved = log_write_batch(chip_id, batch, count, RAINUS_BUILD_UNIX_TIME);
            if (!saved) stop_with_error(ERROR_SD_WRITE);
            // Clear only after BOTH the rows and any collection marker sync.
            batch_count = 0;
#if RAINUS_DEBUG_USB
            saved_count += count; // Sensor tips only; collection markers aren't rain.
#endif
            // The marker follows all tips this acknowledgment covers. Tips
            // queued after it remain in the next batch; this isn't a shutdown.
            if (event.kind == EVENT_FLUSH) acknowledge_flush();
        }
    }
}

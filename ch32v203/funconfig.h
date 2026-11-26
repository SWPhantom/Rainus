#ifndef RAINUS_FUNCONFIG_H
#define RAINUS_FUNCONFIG_H
// ch32fun reads these macro names during chip startup; their spelling is part
// of that library's API. Choose USB diagnostics or the low-power field clock.
#ifndef RAINUS_DEBUG_USB
#define RAINUS_DEBUG_USB 0
#endif
#if RAINUS_DEBUG_USB
// USB needs 48 MHz: use the external 8 MHz crystal (HSE, High-Speed External)
// and multiply it by six with the PLL (Phase-Locked Loop clock multiplier).
// This build stays awake for interactive serial diagnostics.
#define FUNCONF_USE_HSI 0
#define FUNCONF_USE_HSE 1
#define FUNCONF_USE_PLL 1
#define FUNCONF_PLL_MULTIPLIER 6
#define FUNCONF_SYSTEM_CORE_CLOCK 48000000
#define FUNCONF_USE_USBPRINTF 1
#else
// Field: HSI (High-Speed Internal oscillator) supplies 8 MHz, with the external
// main crystal and clock multiplier disabled. STOP wakes on this same clock,
// avoiding a clock-source restoration step after every switch closure. The
// separate low-frequency RTC crystal still runs to keep time during STOP.
#define FUNCONF_USE_HSI 1
#define FUNCONF_USE_HSE 0
#define FUNCONF_USE_PLL 0
#define FUNCONF_PLL_MULTIPLIER 18 // Unused because FUNCONF_USE_PLL is zero.
#define FUNCONF_SYSTEM_CORE_CLOCK 8000000
#define FUNCONF_USE_USBPRINTF 0
#endif
#define FUNCONF_USE_DEBUGPRINTF 0
#define FUNCONF_DEBUG_HARDFAULT 0
#endif

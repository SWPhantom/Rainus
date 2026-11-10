#ifndef RAINUS_FUNCONFIG_H
#define RAINUS_FUNCONFIG_H
#ifndef RAINUS_DEBUG_USB
#define RAINUS_DEBUG_USB 0
#endif
#if RAINUS_DEBUG_USB
// USB needs the 48 MHz clock and stays awake for interactive diagnostics.
#define FUNCONF_USE_HSI 0
#define FUNCONF_USE_HSE 1
#define FUNCONF_USE_PLL 1
#define FUNCONF_PLL_MULTIPLIER 6
#define FUNCONF_SYSTEM_CORE_CLOCK 48000000
#define FUNCONF_USE_USBPRINTF 1
#else
// STOP mode resumes on HSI; keeping HSI as the normal clock avoids a
// PLL/HSE clock restoration step after every switch closure.
#define FUNCONF_USE_HSI 1
#define FUNCONF_USE_HSE 0
#define FUNCONF_USE_PLL 0
#define FUNCONF_PLL_MULTIPLIER 18
#define FUNCONF_SYSTEM_CORE_CLOCK 8000000
#define FUNCONF_USE_USBPRINTF 0
#endif
#define FUNCONF_USE_DEBUGPRINTF 0
#define FUNCONF_DEBUG_HARDFAULT 0
#endif

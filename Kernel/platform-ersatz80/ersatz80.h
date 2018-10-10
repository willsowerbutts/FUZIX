#ifndef __ERSATZ80_DOT_H__
#define __ERSATZ80_DOT_H__

#include "config.h"

#define UART0_BASE 0x00
__sfr __at (UART0_BASE + 0) UART0_STATUS;
__sfr __at (UART0_BASE + 1) UART0_DATA;

#define DISK_BASE 0x20
// registers defined in devdisk.h

#define INT_BASE 0x18
__sfr __at (INT_BASE + 0) INT_STATUS;
__sfr __at (INT_BASE + 1) INT_MASK;
#define INT_BIT_TIMER 0
#define INT_BIT_UART0 1

#define TIMER_BASE 0x1A
__sfr __at (TIMER_BASE + 0) TIMER_CONTROL;

__sfr __at (0x10) USER_LEDS_0; // all 8 bits map to LEDs
__sfr __at (0x11) USER_LEDS_1; // only lowest 4 bits map to LEDs

#define MMU_BASE 0x78
__sfr __at (MMU_BASE + 0) MMU_BANK0; // 0x0000
__sfr __at (MMU_BASE + 1) MMU_BANK1; // 0x4000
__sfr __at (MMU_BASE + 2) MMU_BANK2; // 0x8000
__sfr __at (MMU_BASE + 3) MMU_BANK3; // 0xC000
__sfr __at (MMU_BASE + 4) MMU_FOREIGN0; // 0x0000
__sfr __at (MMU_BASE + 5) MMU_FOREIGN1; // 0x4000
__sfr __at (MMU_BASE + 6) MMU_FOREIGN2; // 0x8000
__sfr __at (MMU_BASE + 7) MMU_FOREIGN3; // 0xC000

#endif

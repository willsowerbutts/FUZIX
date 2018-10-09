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

#endif

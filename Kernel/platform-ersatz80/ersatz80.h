#ifndef __ERSATZ80_DOT_H__
#define __ERSATZ80_DOT_H__

#include "config.h"

#define UART0_BASE 0x00
__sfr __at (UART0_BASE + 0) UART0_STATUS;
__sfr __at (UART0_BASE + 1) UART0_DATA;

#define DISK_BASE 0x20
// registers defined in devdisk.h

#endif

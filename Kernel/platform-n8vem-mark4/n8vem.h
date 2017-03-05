#ifndef __N8VEM_DOT_H__
#define __N8VEM_DOT_H__

#include "config.h"

#ifdef CONFIG_PROPIO2
__sfr __at (PROPIO2_IO_BASE + 0x00) PROPIO2_STAT;
__sfr __at (PROPIO2_IO_BASE + 0x01) PROPIO2_TERM;
#endif

#endif

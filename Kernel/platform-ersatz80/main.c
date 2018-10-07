#include <kernel.h>
#include <kdata.h>
#include <printf.h>
#include <devtty.h>
#include "config.h"

extern unsigned char irqvector;
struct blkbuf *bufpool_end = bufpool + NBUFS; /* minimal for boot -- expanded after we're done with _DISCARD */

void platform_discard(void)
{
    while(bufpool_end < (struct blkbuf*)(KERNTOP - sizeof(struct blkbuf))){
        memset(bufpool_end, 0, sizeof(struct blkbuf));
#if BF_FREE != 0
        bufpool_end->bf_busy = BF_FREE; /* redundant when BF_FREE == 0 */
#endif
        bufpool_end->bf_dev = NO_DEVICE;
        bufpool_end++;
    }
}

void platform_idle(void)
{
	/* Let's go to sleep while we wait for something to interrupt us;
	 * Makes the HALT LED go yellow, which amuses me greatly. */
	__asm
		halt
	__endasm;
}

void platform_interrupt(void)
{
	switch(irqvector) {
		case 1:
			timer_interrupt(); 
			return;
		// case 2:
		// 	tty_pollirq_uart0();
		// 	return;
		default:
			return;
	}
}

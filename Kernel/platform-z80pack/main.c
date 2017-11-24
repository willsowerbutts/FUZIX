#include <kernel.h>
#include <timer.h>
#include <kdata.h>
#include <printf.h>
#include <devtty.h>
#include <devatsim.h>
#include <net_z80pack.h>

uaddr_t ramtop = PROGTOP;

void pagemap_init(void)
{
 int i;
 for (i = 1; i < 8; i++)
  pagemap_add(i);
}

/* On idle we spin checking for the terminals. Gives us more responsiveness
   for the polled ports */
void platform_idle(void)
{
  /* We don't want an idle poll and IRQ driven tty poll at the same moment */
  irqflags_t irq = di();
  tty_pollirq(); 
  irqrestore(irq);
}

void platform_interrupt(void)
{
 tty_pollirq();
 timer_interrupt();
// netat_poll();
// netz_poll();
}

/* Nothing to do for the map of init */
void map_init(void)
{
}

uint8_t platform_param(char *p)
{
 used(p);
 return 0;
}

#ifdef CONFIG_LEVEL_2

/* We always use 512 byte paths so no special pathbuf needed */

char *pathbuf(void)
{
 return tmpbuf();
}

void pathfree(char *tb)
{
 tmpfree(tb);
}

#endif

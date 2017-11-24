/*
  A simple line printer char driver.  
    the only minor number, for now, is 0: the DriveWire Printer.
*/


#include <kernel.h>
#include <version.h>
#include <kdata.h>
#include <device.h>
#include <devlpr.h>


int lpr_open(uint8_t minor, uint16_t flag)
{
	if (minor){
		udata.u_error = ENODEV;
		return -1;
	}
	return 0;
}

int lpr_close(uint8_t minor)
{
	uint8_t b = 0x46;
	if (minor == 0)
		dw_transaction(&b, 1, NULL, 0, 0);
	return 0;
}

static int iopoll(int sofar)
{
	/* Ought to be a core helper for this lot ? */
	if (need_reschedule())
		_sched_yield();
	if (chksigs()) {
		if (sofar)
			return sofar;
		udata.u_error = EINTR;
		return -1;
	}
	return 0;
}


int lpr_write(uint8_t minor, uint8_t rawflag, uint8_t flag)
{
	uint8_t *p = udata.u_base;
	uint8_t *pe = p + udata.u_count;
	int n;
	irqflags_t irq;
	uint8_t buf[2];

	buf[0]=0x50;
	if (minor == 0) {
		while (p < pe) {
			if ((n = iopoll(pe - p)) != 0)
				return n;
			buf[1] = ugetc(p++);
			dw_transaction(buf, 2, NULL, 0, 0);
		}
	} 
	return pe - p;
}

#include <kernel.h>
#include <version.h>
#include <kdata.h>
#include <devlpr.h>

__sfr __at 0x90 lpstat;
__sfr __at 0x91 lpdata;

int lpr_open(uint8_t minor, uint16_t flag)
{
	minor;
	flag;			// shut up compiler
	return 0;
}

int lpr_close(uint8_t minor)
{
	minor;			// shut up compiler
	return 0;
}

int lpr_write(uint8_t minor, uint8_t rawflag, uint8_t flag)
{
	int c = udata.u_count;
	char *p = udata.u_base;

	minor;
	rawflag;
	flag;			// shut up compiler

	while (c-- > 0) {
		while (lpstat & 2) {
			if (need_resched) {
				if (psleep_flags(NULL, flag)) {
					if (udata.u_count)
						udata.u_error = 0;
					return udata.u_count;
				}
			}
		}
		/* Data */
		lpdata = ugetc(p++);
		/* Strobe */
		lpstat |= 1;
		/* FIXME: delay needed */
		lpstat &= ~1;
	}
	return udata.u_count;
}

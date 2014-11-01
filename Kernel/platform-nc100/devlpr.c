#include <kernel.h>
#include <version.h>
#include <kdata.h>
#include <device.h>
#include <devlpr.h>

__sfr __at 0xa0 lpstat;
__sfr __at 0x80 lpstat200;
__sfr __at 0x40 lpdata;

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
	uint16_t ct;

	minor;
	rawflag;
	flag;			// shut up compiler

	while (c-- > 0) {
		ct = 0;

		/* Try and balance polling and sleeping */
#ifdef CONFIG_NC200
		while (lpstat200 & 1) {
#else
		while (lpstat & 2) {
#endif
			ct++;
			if (ct == 10000) {
				udata.u_ptab->p_timeout = 3;
				if (psleep_flags(NULL, flag)) {
					if (udata.u_count)
						udata.u_error = 0;
					return udata.u_count;
				}
				ct = 0;
			}
		}
		/* Data */
		lpdata = ugetc(p++);
		/* Strobe */
		mod_control(0, 0x40);
		mod_control(0x40, 0);
	}
	return udata.u_count;
}

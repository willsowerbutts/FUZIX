#include <kernel.h>
#include <version.h>
#include <kdata.h>
#include <devsys.h>
#include <audio.h>
#include <netdev.h>
#include <devmem.h>
#include <net_native.h>

/*
 *	System devices:
 *
 *	Minor	0	null
 *	Minor 	1	kmem    (kernel memory)
 *	Minor	2	zero
 *	Minor	3	proc
 *	Minor   4       mem     (physical memory)
 *	Minor	64	audio
 *	Minor	65	net_native
 *
 *	Use Minor 128+ for platform specific devices
 */

int sys_read(uint8_t minor, uint8_t rawflag, uint8_t flag)
{
	unsigned char *addr = (unsigned char *) ptab;

	used(rawflag);
	used(flag);

	switch (minor) {
	case 0:
		return 0;
	case 1:
		if (uput((unsigned char *) udata.u_offset, udata.u_base,
			       udata.u_count))
			return -1;
		return udata.u_count;
	case 2:
		if (udata.u_sysio)
			memset(udata.u_base, 0, udata.u_count);
		else
			uzero(udata.u_base, udata.u_count);
		return udata.u_count;
	case 3:
		if (udata.u_count > sizeof(struct p_tab))
			udata.u_count = sizeof(struct p_tab);
		if (udata.u_offset + udata.u_count > PTABSIZE * sizeof(struct p_tab))
			return 0;
		if (uput(addr + udata.u_offset, udata.u_base, udata.u_count))
			return -1;
		return udata.u_count;
#ifdef CONFIG_DEV_MEM
        case 4:
                return devmem_read();
#endif
#ifdef CONFIG_RTC_FULL
	case 5:
		return platform_rtc_read();
#endif
#ifdef CONFIG_NET_NATIVE
	case 65:
		return netdev_read(flag);
#endif
	default:
		udata.u_error = ENXIO;
		return -1;
	}
}

int sys_write(uint8_t minor, uint8_t rawflag, uint8_t flag)
{
	used(rawflag);
	used(flag);

	switch (minor) {
	case 0:
	case 2:
		return udata.u_count;
	case 1:
		if(uget((unsigned char *) udata.u_offset, udata.u_base,
			       udata.u_count))
			return -1;
		return udata.u_count;
	case 3:
		udata.u_error = EINVAL;
		return -1;
#ifdef CONFIG_DEV_MEM
        case 4:
                return devmem_write();
#endif
#ifdef CONFIG_RTC_FULL
	case 5:
		return platform_rtc_write();
#endif
#ifdef CONFIG_NET_NATIVE
	case 65:
		return netdev_write(flag);
#endif
	default:
		udata.u_error = ENXIO;
		return -1;
	}
}

#define PIO_TABSIZE	1
#define PIO_ENTRYSIZE	2

int sys_ioctl(uint8_t minor, uarg_t request, char *data)
{
#ifdef CONFIG_AUDIO
	if (minor == 64)
		return audio_ioctl(request, data);
#endif
#ifdef CONFIG_NET_NATIVE
	if (minor == 65)
		return netdev_ioctl(request, data);
#endif
	if (minor != 3) {
		udata.u_error = ENOTTY;
		return -1;
	}

	switch (request) {
	case PIO_TABSIZE:
		uputw(maxproc, data);
		break;

	case PIO_ENTRYSIZE:
		uputw(sizeof(struct p_tab), data);
		break;

	default:
		udata.u_error = EINVAL;
		return (-1);
	}
	return 0;
}

int sys_close(uint8_t minor)
{
	used(minor);
#ifdef CONFIG_NET_NATIVE
	if (minor == 65)
		return netdev_close(minor);
#endif
	return 0;
}

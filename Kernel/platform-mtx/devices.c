#include <kernel.h>
#include <version.h>
#include <kdata.h>
#include <tty.h>
#include <vt.h>
#include <devfd.h>
#include <devsil.h>
#include <devsys.h>
#include <devlpr.h>
#include <devtty.h>

struct devsw dev_tab[] =  /* The device driver switch table */
{
// minor    open         close        read      write       ioctl
// -----------------------------------------------------------------
  /* 0: /dev/fd		Floppy disc block devices  */
  {  fd_open,      no_close,    fd_read,  fd_write,   no_ioctl  },
  /* 1: /dev/hd		Hard disc block devices (hdx - not supported yet) */
  {  no_open,      no_close,    no_rdwr,  no_rdwr,   no_ioctl   },
  /* 2: /dev/tty	TTY devices */
  {  tty_open,     mtxtty_close,tty_read, tty_write,  vt_ioctl },
  /* 3: /dev/lpr	Printer devices */
  {  lpr_open,     lpr_close,   no_rdwr,  lpr_write,  no_ioctl  },
  /* 4: /dev/mem etc	System devices (one offs) */
  {  no_open,      no_close,    sys_read, sys_write,  sys_ioctl },
  /* Pack to 7 with nxio if adding private devices and start at 8 */
  {  no_open,      no_close,    no_rdwr,  no_rdwr,    no_ioctl  },
  {  no_open,      no_close,    no_rdwr,  no_rdwr,    no_ioctl  },
  {  no_open,      no_close,    no_rdwr,  no_rdwr,    no_ioctl  },
  /* 8: /dev/sild	Silicon disc block devices  */
  {  sil_open,     no_close,    sil_read, sil_write,  no_ioctl  },
};

bool validdev(uint16_t dev)
{
    /* This is a bit uglier than needed but the right hand side is
       a constant this way */
    if(dev > ((sizeof(dev_tab)/sizeof(struct devsw)) << 8) - 1)
	return false;
    else
        return true;
}

void device_init(void)
{
//  int i;
//  for (i = 0; i < MAX_SWAPS; i++)
//   swapmap_init(i);
}

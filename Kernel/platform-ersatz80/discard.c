#include <kernel.h>
#include <kdata.h>
#include <printf.h>
#include <devtty.h>
#include <ds1302.h>
#include "config.h"
#include "devrd.h"

/* Everything in here is discarded after init starts */

#ifdef CONFIG_PPIDE
#include <devide.h>
void ppide_init(void);
#endif

void init_hardware_c(void)
{
    ramsize = 1024;
    procmem = 1024 - 64;
    /* zero out the initial bufpool */
    memset(bufpool, 0, (char*)bufpool_end - (char*)bufpool);
}

void pagemap_init(void)
{
    int i;

    /* hardware knows how much RAM is fitted but we don't have any way to read it out yet! */
    for (i = 4; i < 64; i++)
        pagemap_add(i);

    /* finally add the common area */
    pagemap_add(3);
}

void map_init(void)
{
}

void device_init(void)
{
    devdisk_init();
}

uint8_t platform_param(char *p)
{
    used(p);
    return 0;
}

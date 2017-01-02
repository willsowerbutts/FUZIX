/* Zeta SBC V2 /dev/physmem driver
 *
 * 2017-01-02 William R Sowerbutts
 *
 * A closer merge with the RAM disk driver would be nice
 */

#include <kernel.h>
#include <kdata.h>
#include <printf.h>
#define DEVRD_PRIVATE
#include "devrd.h"

int devphysmem_transfer(void);

int devphysmem_read(void)
{
    rd_reverse = false;
    return devphysmem_transfer();
}

int devphysmem_write(void)
{
    rd_reverse = true;
    return devphysmem_transfer();
}

int devphysmem_transfer(void)
{
    uint16_t buffer_addr;
    off_t mem_addr;
    usize_t xfer_count, t;

    mem_addr = udata.u_offset;
    xfer_count = udata.u_count;
    buffer_addr = (uint16_t) udata.u_base;

    while(true){
        rd_dst_page = ((uint8_t *)&udata.u_page)[buffer_addr >> 14];
        rd_dst_offset = buffer_addr & 0x3FFF;
        rd_src_page = mem_addr >> 14;
        rd_src_offset = mem_addr & 0x3FFF;
        rd_cpy_count = xfer_count;
        t = 0x4000 - ( (rd_src_offset > rd_dst_offset) ? rd_src_offset : rd_dst_offset );
        if(rd_cpy_count > t)
            rd_cpy_count = t;

#ifdef DEBUG
        kprintf("physmem: %x %x %s %x %x, count %d\n",
                rd_src_page, rd_src_offset,
                rd_reverse ? "<-w-" : "-r->",
                rd_dst_page, rd_dst_offset,
                rd_cpy_count);
#endif

        rd_page_copy();
        xfer_count -= rd_cpy_count;
        if(!xfer_count)
            return udata.u_count;
        buffer_addr += rd_cpy_count;
        mem_addr += rd_cpy_count;
    }
}

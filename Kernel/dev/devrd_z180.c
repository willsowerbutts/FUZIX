/* Z180 (Mark IV SBC & P112) memory driver
 *
 *     minor 0: /dev/rd0   ROM disk block device
 *     minor 1: /dev/rd1   RAM disk block device
 *
 * 2017-01-05 William R Sowerbutts, based on Zeta-v2 RAM disk code by Sergey Kiselev
 */

#include <kernel.h>
#include <kdata.h>
#include <printf.h>
#define DEVRD_PRIVATE
#include "devrd_z180.h"

static const uint32_t dev_limit[NUM_DEV_RD] = {
    (uint32_t)(DEV_RD_ROM_PAGES + DEV_RD_ROM_START) << 12, /* block /dev/rd0: ROM */
    (uint32_t)(DEV_RD_RAM_PAGES + DEV_RD_RAM_START) << 12, /* block /dev/rd1: RAM */
};

static const uint32_t dev_start[NUM_DEV_RD] = {
    (uint32_t)DEV_RD_ROM_START << 12,                      /* block /dev/rd0: ROM */
    (uint32_t)DEV_RD_RAM_START << 12,                      /* block /dev/rd1: RAM */
};

int rd_transfer(uint8_t minor, uint8_t rawflag, uint8_t flag) /* implements both rd_read and rd_write */
{
    bool error = false;

    used(flag);

    /* check device exists; do not allow writes to ROM */
    if(minor >= NUM_DEV_RD || (minor == RD_MINOR_ROM && rd_reverse)){
        error = true;
    }else{
        rd_src_address = dev_start[minor];

        if(rawflag){
            /* rawflag == 1, userspace transfer */
            rd_dst_userspace = true;
            rd_dst_address = (uint16_t)udata.u_base;
            rd_src_address += udata.u_offset;
            rd_cpy_count = udata.u_count;
        }else{
            /* rawflag == 0, kernel transfer */
            rd_dst_userspace = false;
            rd_dst_address = (uint16_t)&udata.u_buf->bf_data;
            rd_src_address += ((uint32_t)udata.u_buf->bf_blk << 9);
            rd_cpy_count = 512;
        }

        if(rd_src_address >= dev_limit[minor]){
            error = true;
        }
    }

    if(error){
        udata.u_error = EIO;
        return -1;
    }

    rd_page_copy();

    return rd_cpy_count >> 9;
}

int rd_open(uint8_t minor, uint16_t flags)
{
    flags; /* unused */

    switch(minor){
#if DEV_RD_RAM_PAGES > 0
        case RD_MINOR_RAM:
#endif
#if DEV_RD_ROM_PAGES > 0
        case RD_MINOR_ROM:
#endif
            return 0;
        default:
            udata.u_error = ENXIO;
            return -1;
    }
}

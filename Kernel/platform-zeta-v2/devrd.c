/* Zeta SBC V2 RAM disk driver 
 *
 * Implements a single RAM disk DEV_RD_PAGES size and
 * starting from DEV_RD_START page
 *
 * */

#include <kernel.h>
#include <kdata.h>
#include <printf.h>
#include "devrd.h"

extern uint8_t rd_src_page;     /* source page number */
extern uint8_t rd_dst_page;     /* destination page number */
extern uint16_t rd_src_offset;  /* offset of the data in the source page */
extern uint16_t rd_dst_offset;  /* offset of the data in the destination page */
extern uint16_t rd_cpy_count;   /* data transfer length */
extern uint8_t kernel_pages[];  /* kernel's page table */

int ramdisk_transfer(bool is_read, uint8_t minor, uint8_t rawflag);
void rd_page_copy(void);        /* assembler code */

int rd_read(uint8_t minor, uint8_t rawflag, uint8_t flag)
{
    flag;
    return ramdisk_transfer(true, minor, rawflag);
}

int rd_write(uint8_t minor, uint8_t rawflag, uint8_t flag)
{
    flag;
    return ramdisk_transfer(false, minor, rawflag);
}

int ramdisk_transfer(bool is_read, uint8_t minor, uint8_t rawflag)
{
#if (DEV_RD_RAM_PAGES+DEV_RD_ROM_PAGES) == 0
    is_read; /* unused */
    minor;   /* unused */
    rawflag; /* unused */
    
    /* neither device is present -- just return an error */
    udata.u_error = ENXIO;
    return -1;
#else
    blkno_t block;
    int block_xfer;     /* r/w return value (number of 512 byte blocks transferred) */
    uint16_t buffer_addr;
    uint8_t disk_page_nr;
    uint16_t disk_page_offset;
    uint16_t t;
    usize_t xfer_count;
    bool error = false;

    if(minor > 1){
        udata.u_error = ENXIO;
        return -1;
    }

    if (rawflag) { /* rawflag == 1, read to or write from user space */
        xfer_count = udata.u_count;
        buffer_addr = (uint16_t) udata.u_base;
        block = udata.u_offset >> 9;
        block_xfer = xfer_count >> 9;
    } else { /* rawflag == 0, read to or write from kernel space */
        xfer_count = 512;
        buffer_addr = (uint16_t) udata.u_buf->bf_data;
        block = udata.u_buf->bf_blk;
        block_xfer = 1;
    }

    disk_page_nr = block >> 5;
    disk_page_offset = (block & 0x1F) << 9;

    if(minor == RD_MINOR_RAM){
        disk_page_nr += DEV_RD_RAM_START;
        t = DEV_RD_RAM_PAGES << 5;
    }else{ /* minor == RD_MINOR_ROM */
        if(!is_read) /* ROM is read-only */
            error = true;
        disk_page_nr += DEV_RD_ROM_START;
        t = DEV_RD_ROM_PAGES << 5;
    }

    /* check we don't overrun the device */
    if(block >= t)
        error = true;

    if(error){
        udata.u_error = EIO;
        return -1;
    }

    while (xfer_count > 0) {
        /* split buffer_addr into page and offset, lookup page number in user/kernel page tables */
        rd_dst_page = (rawflag ? ((uint8_t *)&udata.u_page) : kernel_pages)[buffer_addr >> 14];
        rd_dst_offset = buffer_addr & 0x3FFF;

        if (is_read) {
            rd_src_page = disk_page_nr;
            rd_src_offset = disk_page_offset;
        } else {
            /* move calculated values into correct variables */
            rd_src_page = rd_dst_page;
            rd_src_offset = rd_dst_offset;
            rd_dst_page = disk_page_nr;
            rd_dst_offset = disk_page_offset;
        }

        rd_cpy_count = xfer_count;

        /* ensure we don't cross any 16K bank boundaries */
        t = 0x4000 - ( (rd_src_offset > rd_dst_offset) ? rd_src_offset : rd_dst_offset );
        if(rd_cpy_count > t)
            rd_cpy_count = t;
#ifdef DEBUG
        kprintf("page_cpy(src_page=%x, src_offset=%x, dst_page=%x, dst_offset=%x, cpy_count=%x)\n", src_page, src_offset, dst_page, dst_offset, cpy_count);
#endif
        rd_page_copy();
        xfer_count -= rd_cpy_count;
        buffer_addr += rd_cpy_count;
        disk_page_offset += rd_cpy_count;
        if(disk_page_offset >= 0x4000){ /* overflowed into the next disk page? */
            disk_page_offset &= 0x3FFF;
            disk_page_nr++;
        }
    }

    return block_xfer;
#endif
}


int rd_open(uint8_t minor, uint16_t flags)
{
    flags; /* unused */

#if (DEV_RD_RAM_PAGES+DEV_RD_ROM_PAGES) == 0
    minor; /* unused */

    udata.u_error = EIO;
    return -1;
#else
    switch(minor){
#if DEV_RD_RAM_PAGES > 0
        case RD_MINOR_RAM:
#endif
#if DEV_RD_ROM_PAGES > 0
        case RD_MINOR_ROM:
#endif
            return 0;
        default:
            udata.u_error = EIO;
            return -1;
    }
#endif
}

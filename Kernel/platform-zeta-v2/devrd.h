#ifndef __DEVRD_DOT_H__
#define __DEVRD_DOT_H__

#include "config.h"

#define NUM_DEV_RD 2
#define RD_MINOR_RAM 0
#define RD_MINOR_ROM 1

#define DEV_RD_ROM_START (32-DEV_RD_ROM_PAGES) /* first page used by the ROM disk */
#define DEV_RD_RAM_START (64-DEV_RD_RAM_PAGES) /* first page used by the RAM disk */

/* public interface */
int rd_read(uint8_t minor, uint8_t rawflag, uint8_t flag);
int rd_write(uint8_t minor, uint8_t rawflag, uint8_t flag);
int rd_init(void);
int rd_open(uint8_t minor, uint16_t flags);

#ifdef DEVRD_PRIVATE
extern uint8_t rd_src_page;     /* source page number */
extern uint8_t rd_dst_page;     /* destination page number */
extern uint16_t rd_src_offset;  /* offset of the data in the source page */
extern uint16_t rd_dst_offset;  /* offset of the data in the destination page */
extern uint16_t rd_cpy_count;   /* data transfer length */
extern uint8_t rd_reverse;      /* reverse the copy direction -- read - false, write - true */
extern uint8_t kernel_pages[];  /* kernel's page table */
void rd_page_copy(void);        /* in devrd_hw.s */
#endif

#endif /* __DEVRD_DOT_H__ */

#include <kernel.h>
#include <printf.h>
#include <kdata.h>
#include <ersatz80.h>
#include <devdisk.h>
#include <blkdev.h>

uint8_t devdisk_transfer_sector(void)
{
    uint8_t *p = ((uint8_t *)&blk_op.lba)+2;
    uint8_t *pages;

    DISK_COMMAND = DISK_CMD_SELECT_DRIVE(blk_op.blkdev->driver_data);
    DISK_SECTOR_COUNT = blk_op.nblock;
    /* sdcc sadly unable to figure this out for itself yet */
    DISK_SEC_2 = *(p--);
    DISK_SEC_1 = *(p--);
    DISK_SEC_0 = *(p--);

    if(blk_op.is_user){
        pages = (uint8_t *) & udata.u_page;
        MMU_FOREIGN0 = pages[0];
        MMU_FOREIGN1 = pages[1];
        MMU_FOREIGN2 = pages[2];
        MMU_FOREIGN3 = pages[3];
        DISK_DMA_2 = 0x80; // userspace transfer - use foreign MMU context
    }else{
        DISK_DMA_2 = 0x00; // kernel transfer - use current MMU context
    }
    DISK_DMA_1 = ((uint16_t)blk_op.addr) >> 8;
    DISK_DMA_0 = ((uint16_t)blk_op.addr);

    if(blk_op.is_read){
        USER_LEDS_1 = 0x01;
        DISK_COMMAND = DISK_CMD_READ;
        USER_LEDS_1 = 0x00;
    }else{
        USER_LEDS_1 = 0x02;
        DISK_COMMAND = DISK_CMD_WRITE;
        USER_LEDS_1 = 0x00;
    }

    return blk_op.nblock;
}

int devdisk_sync(void)
{
    DISK_COMMAND = DISK_CMD_SELECT_DRIVE(blk_op.blkdev->driver_data);
    DISK_COMMAND = DISK_CMD_SYNC;
    return 0;
}

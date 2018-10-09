#include <kernel.h>
#include <printf.h>
#include <kdata.h>
#include <ersatz80.h>
#include <devdisk.h>
#include <blkdev.h>

uint8_t devdisk_transfer_sector(void)
{
    uint8_t *p = ((uint8_t *)&blk_op.lba)+2;

    DISK_COMMAND = DISK_CMD_SELECT_DRIVE(blk_op.blkdev->driver_data);
    DISK_SECTOR_COUNT = blk_op.nblock;
    /* sdcc sadly unable to figure this out for itself yet */
    DISK_SEC_2 = *(p--);
    DISK_SEC_1 = *(p--);
    DISK_SEC_0 = *(p--);

    if(blk_op.is_user){
        panic("devdisk_transfer_sector: user transfers not yet supported\n");
        DISK_DMA_2 = 0x80; // enable foreign DMA mode
        // also program the foreign DMA MMU registers
        // need to ensure an interrupt doesn't come along and switch us to another task ... ?
    }else{
        // kernel transfer
        DISK_DMA_2 = 0;
    }
    DISK_DMA_1 = ((uint16_t)blk_op.addr) >> 8;
    DISK_DMA_0 = ((uint16_t)blk_op.addr);

    if(blk_op.is_read)
        DISK_COMMAND = DISK_CMD_READ;
    else
        DISK_COMMAND = DISK_CMD_WRITE;

    return blk_op.nblock;
}

int devdisk_sync(void)
{
    DISK_COMMAND = DISK_CMD_SELECT_DRIVE(blk_op.blkdev->driver_data);
    DISK_COMMAND = DISK_CMD_SYNC;
    return 0;
}

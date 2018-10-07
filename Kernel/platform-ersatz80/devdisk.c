#include <kernel.h>
#include <printf.h>
#include <kdata.h>
#include <ersatz80.h>
#include <devdisk.h>
#include <blkdev.h>

uint8_t devdisk_transfer_sector(void)
{
#if defined(__SDCC_z80) || defined(__SDCC_z180) || defined(__SDCC_gbz80) || defined(__SDCC_r2k) || defined(__SDCC_r3k)
    uint8_t *p;
#endif

    DISK_COMMAND = DISK_CMD_SELECT_DRIVE(blk_op.blkdev->driver_data);
#if defined(__SDCC_z80) || defined(__SDCC_z180) || defined(__SDCC_gbz80) || defined(__SDCC_r2k) || defined(__SDCC_r3k)
    p = ((uint8_t *)&blk_op.lba)+2;
    /* sdcc sadly unable to figure this out for itself yet */
    DISK_SEC_2 = *(p--);
    DISK_SEC_1 = *(p--);
    DISK_SEC_0 = *(p--);
#else
    DISK_SEC_0 = blk_op.lba;
    DISK_SEC_1 = blk_op.lba >> 8;
    DISK_SEC_2 = blk_op.lba >> 16;
#endif
    DISK_SECTOR_COUNT = blk_op.nblock;

    if(blk_op.is_user){
        while(1)
            kputs("devdisk_transfer_sector: user transfers not yet supported\n");
    }else{
        // kernel transfer
        DISK_DMA_2 = 0;
        DISK_DMA_1 = ((uint16_t)blk_op.addr) >> 8;
        DISK_DMA_0 = ((uint16_t)blk_op.addr);
    }

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

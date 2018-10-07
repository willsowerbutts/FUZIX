#include <kernel.h>
#include <printf.h>
#include <kdata.h>
#include <ersatz80.h>
#include <devdisk.h>
#include <blkdev.h>

/* code in here ends up in DISCARD and is not available after booting */

void devdisk_init_drive(uint8_t drive)
{
    blkdev_t *blk;

    // check if the drive is present and the disk is mounted
    // absent drives always have the error flag set
    // unmounted drives also always have the error flag set
    DISK_COMMAND = DISK_CMD_SELECT_DRIVE(drive);
    DISK_COMMAND = DISK_CMD_CLEAR_ERROR;
    if(DISK_STATUS & DISK_STATUS_ERROR)
        return;

    blk = blkdev_alloc();
    if(!blk)
        return;
    // we use 512-byte sectors
    DISK_COMMAND = DISK_CMD_SECTOR_SIZE_512;
    blk->driver_data = drive;
    blk->transfer = devdisk_transfer_sector;
    blk->flush = devdisk_sync;
    // read out the device size
    DISK_COMMAND = DISK_CMD_SEEK_END;
    blk->drive_lba_count = (((uint32_t)DISK_SEC_0) | 
                            ((uint32_t)DISK_SEC_1 << 8) |
                            ((uint32_t)DISK_SEC_2 << 16)) + 1;
    kprintf("ersatz80 disk %d (%dMB) ", drive, (int)(blk->drive_lba_count >> 11));
    blkdev_scan(blk, SWAPSCAN);
}

void devdisk_init(void)
{
    uint8_t d;

    for(d=0; d<DEV_DISK_DRIVE_COUNT; d++)
        devdisk_init_drive(d);
}

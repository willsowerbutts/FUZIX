#ifndef __DEVDISK_DOT_H__
#define __DEVDISK_DOT_H__

#include <ersatz80.h>

#define DEV_DISK_DRIVE_COUNT 16 // max drives hardware supports

void devdisk_init(void);
int devdisk_sync(void);
uint8_t devdisk_transfer_sector(void);

__sfr __at (DISK_BASE + 1) DISK_SEC_2;
__sfr __at (DISK_BASE + 2) DISK_SEC_1;
__sfr __at (DISK_BASE + 3) DISK_SEC_0;
__sfr __at (DISK_BASE + 4) DISK_DMA_2;
__sfr __at (DISK_BASE + 5) DISK_DMA_1;
__sfr __at (DISK_BASE + 6) DISK_DMA_0;
__sfr __at (DISK_BASE + 7) DISK_SECTOR_COUNT;
__sfr __at (DISK_BASE + 8) DISK_STATUS;
__sfr __at (DISK_BASE + 8) DISK_COMMAND;

#define DISK_STATUS_WRITABLE      (0x40)
#define DISK_STATUS_ERROR         (0x80)

#define DISK_CMD_SELECT_DRIVE(n)  (0x00 + n)
#define DISK_CMD_SECTOR_SIZE_128  (0x10)
#define DISK_CMD_SECTOR_SIZE_256  (0x11)
#define DISK_CMD_SECTOR_SIZE_512  (0x12)
#define DISK_CMD_SECTOR_SIZE_1024 (0x13)
#define DISK_CMD_READ             (0x20)
#define DISK_CMD_WRITE            (0x21)
#define DISK_CMD_MOUNT            (0x22)
#define DISK_CMD_UNMOUNT          (0x23)
#define DISK_CMD_SEEK_END         (0x24)
#define DISK_CMD_REPORT_SIZE      (0x25)
#define DISK_CMD_SYNC             (0x26)
#define DISK_CMD_CLEAR_ERROR      (0x80)

#endif

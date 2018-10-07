/* Enable to make ^Z dump the inode table for debug */
#undef CONFIG_IDUMP
/* Enable to make ^A drop back into the monitor */
#undef CONFIG_MONITOR
/* Profil syscall support (not yet complete) */
#undef CONFIG_PROFIL
/* Multiple processes in memory at once */
#define CONFIG_MULTI
/* Single tasking */
#undef CONFIG_SINGLETASK
/* CP/M emulation */
#undef CONFIG_CPM_EMU
/* Flexible 4x16K banking */
#define CONFIG_BANK16
/* Permit large I/O requests to bypass cache and go direct to userspace */
#define CONFIG_LARGE_IO_DIRECT

/* 64 x 16K pages, 3 pages for kernel */
#define MAX_MAPS	(64 - 3)

/* Banks as reported to user space */
#define CONFIG_BANKS	4

#define TICKSPERSEC 20      /* Ticks per second */
#define PROGBASE    0x0000  /* also data base */
#define PROGLOAD    0x0100  /* also data base */
#define PROGTOP     0xF000  /* Top of program, base of U_DATA copy */
#define KERNTOP     0xC000  /* Top of kernel (first 3 banks), base of shared bank */
#define PROC_SIZE   64	  /* Memory needed per process */

/* WRS: this is probably wrong -- we want to swap the full 64K minus the common code */
/* For now let's just use something and fix this up later when we have a swap device */
#define SWAP_SIZE   0x7F 	/* 63.5K in blocks (which is the wrong number) */
#define SWAPBASE    0x0000	/* start at the base of user mem */
#define SWAPTOP	    0xFF00	/* can we stop at the top? not sure how. let's stop short. */
#define MAX_SWAPS	10	    /* Well, that depends really, hmmmmmm. Pick a number, any number. */

/* We need a tidier way to do this from the loader */
#define CMDLINE	NULL  /* Location of root dev name */
#define BOOTDEVICENAMES "hd#,,,rd"

//#define SWAPDEV  (256 + 1)  /* Device for swapping */
#define CONFIG_DYNAMIC_BUFPOOL /* we expand bufpool to overwrite the _DISCARD segment at boot */
#define NBUFS    4        /* Number of block buffers, keep in line with space reserved in ersatz80.s */
#define NMOUNTS	 4	  /* Number of mounts at a time */

#define MAX_BLKDEV 4	    /* 1 ROM disk, 1 RAM disk, 1 floppy, 1 PPIDE */

#define NUM_DEV_TTY 1

/* UART0 as the console */
#define BOOT_TTY (512 + 1)
#define TTY_INIT_BAUD B115200

#define TTYDEV   BOOT_TTY /* Device used by kernel for messages, panics */

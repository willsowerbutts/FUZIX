#include <kernel.h>
#include <timer.h>
#include <kdata.h>
#include <printf.h>
#include <devtty.h>
#include <ttydw.h>

#define DISC __attribute__((section(".discard")))

unsigned int swapdev = 0;
struct blkbuf *bufpool_end = bufpool + NBUFS;


void platform_discard(void)
{
	extern uint8_t discard_size;
	bufptr bp = bufpool_end;

	kprintf("%d buffers reclaimed from discard\n", discard_size);
	
	bufpool_end += discard_size;

	memset( bp, 0, discard_size * sizeof(struct blkbuf) );

	for( bp = bufpool + NBUFS; bp < bufpool_end; ++bp ){
		bp->bf_dev = NO_DEVICE;
		bp->bf_busy = BF_FREE;
	}
}


void platform_idle(void)
{
}

void do_beep(void)
{
}

/* Scan memory return number of 8k pages found */
DISC
int scanmem(void)
{
	volatile uint8_t *mmu=(uint8_t *)0xffa8;
	volatile uint8_t *ptr=(uint8_t *)0x0100;
	int i;
	for( i = 0; i<256; i+=16 ){
		*mmu=i;
		*ptr=0;
	}
	*mmu=0;
	*ptr=0xff;
	*mmu=16;
	for( i = 16; i<256 && !*ptr ; ){
		i+=16;
		*mmu=i;;
	}
	*mmu=0;
	return i;
}


/*
 Map handling: We have flexible paging. Each map table consists
 of a set of pages with the last page repeated to fill any holes.
 */
DISC
void pagemap_init(void)
{
    int i;
    int max = scanmem();
    
    /*  We have 64 8k pages for a CoCo3 so insert every other one
     *  into the kernel allocator map.
     */
    for (i = 12; i < max ; i+=2)
        pagemap_add(i);
    /* add common page last so init gets it */
    pagemap_add(6);
    /* initialize swap pages */
    for (i = 0; i<MAX_SWAPS; i++)
	swapmap_init(i);
}

DISC
void map_init(void)
{
}

DISC
uint8_t platform_param(char *p)
{
	if (!strcmp(p,"NODW") ){
	    dwtype = DWTYPE_NOTFOUND;
	    return -1;
	}
	if (!strncmp(p,"SWAP=",5)){
	    swapdev = bootdevice(p+5);
	    return -1;
	}
	return 0;
}

#ifdef CONFIG_LEVEL_2

/* We always use 512 byte paths so no special pathbuf needed */

char *pathbuf(void)
{
	return tmpbuf();
}

void pathfree(char *tb)
{
	tmpfree(tb);
}

#endif

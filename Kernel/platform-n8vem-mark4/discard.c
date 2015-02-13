#include <kernel.h>
#include <kdata.h>
#include <printf.h>
#include <devtty.h>
#include "config.h"
#include <z180.h>

void pagemap_init(void)
{
    int i;

    /* N8VEM SBC Mark IV has RAM in the top 512K of physical memory. 
     * First 64K is used by the kernel. 
     * Each process gets the full 64K for now.
     * Page size is 4KB. */
    for(i = 0x90; i < 0x100; i+=0x10)
        pagemap_add(i);
}

uint16_t bootdevice(unsigned char *s)
{
    unsigned int r = 0;
    unsigned char c;

    /* skip spaces */
    while(*s == ' ')
        s++;

    /* we're expecting one of;
     * hdX<num>
     * fd<num>
     * <num>
     */

    switch(*s | 32){
        case 'f': /* fd */
            r += 256;
        case 'h': /* hd */
            s++;
            if((*s | 32) != 'd')
                return -1;
            s++;
            if(r == 0){ /* hdX */
                c = (*s | 32) - 'a';
                if(c & 0xF0)
                    return -1;
                r += c << 4;
                s++;
            }
            break;
        default:
            break;
    }

    while(*s >= '0' && *s <= '9'){
        r = (r*10) + (*s - '0');
        s++;
    }

    return r;
}

void map_init(void)
{
    /* clone udata and stack into a regular process bank, return with common memory
       for the new process loaded */
    copy_and_map_process(&init_process->p_page);
    /* kernel bank udata (0x300 bytes) is never used again -- could be reused? */
}

#include <kernel.h>
#include <kdata.h>
#include <printf.h>
#include <stdbool.h>
#include <tty.h>
#include <devtty.h>
#include <ersatz80.h>

char tbuf1[TTYSIZ];

unsigned char uart0_type;

struct  s_queue  ttyinq[NUM_DEV_TTY+1] = {       /* ttyinq[0] is never used */
    {NULL,	NULL,	NULL,	0,	0,	0},
    {tbuf1,	tbuf1,	tbuf1,	TTYSIZ,	0,	TTYSIZ/2},
};

void tty_setup(uint8_t minor)
{
    used(minor);
    // just a NOP on this hardware
}

int tty_carrier(uint8_t minor)
{
    used(minor);
    return 1;
}

void tty_putc(uint8_t minor, unsigned char c)
{
    if (minor == 1) {
        while(UART0_STATUS & 0x40);
        UART0_DATA = c;
    }
}

void tty_sleeping(uint8_t minor)
{
    used(minor);
    // NOP
}

ttyready_t tty_writeready(uint8_t minor)
{
    if(minor == 1){
        if(UART0_STATUS & 0x40)
            return TTY_READY_SOON;
        else
            return TTY_READY_NOW;
    }
    return TTY_READY_NOW;
}

/* kernel writes to system console -- never sleep! */
void kputchar(char c)
{
    tty_putc(TTYDEV - 512, c);
    if(c == '\n')
        tty_putc(TTYDEV - 512, '\r');
}

#ifndef __DEVTTY_DOT_H__
#define __DEVTTY_DOT_H__
void tty_putc(uint8_t minor, unsigned char c);
void tty_pollirq_asci0(void);
void tty_pollirq_asci1(void);

#ifdef CONFIG_PROPIO2
void tty_poll_propio2(void);
#endif
#ifdef CONFIG_ECB_USB_FIFO_TTY
void tty_pollirq_usb_fifo(void);
#endif

#endif

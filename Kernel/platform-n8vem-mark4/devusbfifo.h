#ifndef __DEV_USB_FIFO_DOT_H__
#define __DEV_USB_FIFO_DOT_H__

#include "config.h"

__sfr __at (ECB_USB_FIFO_IO_BASE + 0)   USB_FIFO_DATA;           /* read/write */
__sfr __at (ECB_USB_FIFO_IO_BASE + 1)   USB_FIFO_STATUS;         /* read/write */
__sfr __at (ECB_USB_FIFO_IO_BASE + 2)   USB_FIFO_SEND_IMMEDIATE; /* write only */

/* bits in USB_FIFO_STATUS register */
#define USB_FIFO_STATUS_TXF             0x01 /* 0 = transmit FIFO space available, 1 = transmit FIFO full */
#define USB_FIFO_STATUS_INT_ENABLE      0x02 /* 1 = interrupt request enabled */
#define USB_FIFO_STATUS_INT_TXE         0x04 /* 1 = request interrupt on transmit FIFO space available */
#define USB_FIFO_STATUS_INT_RXF         0x08 /* 1 = request interrupt on receive FIFO data waiting */
#define USB_FIFO_STATUS_GPIO_IN         0x10 /* GPIO bit: data from ECB to USB (read/write) */
#define USB_FIFO_STATUS_GPIO_OUT        0x20 /* GPIO bit: data from USB to ECB (read only) */
#define USB_FIFO_STATUS_IRQ             0x40 /* 1 when interrupt would be requested (regardless of INT_ENABLE bit) */
#define USB_FIFO_STATUS_RXE             0x80 /* 0 = receive FIFO data waiting, 1 = receive FIFO empty */

void usb_fifo_init(void);

#endif

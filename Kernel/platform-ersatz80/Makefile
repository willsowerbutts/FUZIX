ASRCS = crt0.s tricks.s commonmem.s ersatz80.s monitor.s
CSRCS = devices.c main.c devtty.c devdisk.c
DISCARD_CSRCS = discard.c devdisk-discard.c
DSRCS = ../dev/mbr.c ../dev/blkdev.c
DASRCS = 

AOBJS = $(ASRCS:.s=.rel)
COBJS = $(CSRCS:.c=.rel)
DISCARD_COBJS = $(DISCARD_CSRCS:.c=.rel)
DISCARD_DOBJS = $(patsubst ../dev/%.c,%.rel, $(DISCARD_DSRCS))
DOBJS = $(patsubst ../dev/%.c,%.rel, $(DSRCS))
DAOBJS = $(patsubst ../dev/%.s,%.rel, $(DASRCS))

OBJS  = $(AOBJS) $(COBJS) $(DOBJS) $(DAOBJS) $(DISCARD_DOBJS) $(DISCARD_COBJS)

CROSS_CCOPTS += -I../dev/

JUNK = *.rel *.lst *.asm *.sym *.rst *.map *.ihx *.bin

all:	$(OBJS)

$(AOBJS): %.rel: %.s
	$(CROSS_AS) $(ASOPTS) $<

$(COBJS): %.rel: %.c
	$(CROSS_CC) $(CROSS_CCOPTS) -c $<

$(DOBJS): %.rel: ../dev/%.c
	$(CROSS_CC) $(CROSS_CCOPTS) -c $<

$(DAOBJS): %.rel: ../dev/%.s
	$(CROSS_AS) $(ASOPTS) $@ $<

$(DISCARD_COBJS): %.rel: %.c
	$(CROSS_CC) $(CROSS_CCOPTS) $(CROSS_CC_SEGDISC) -c $<

$(DISCARD_DOBJS): %.rel: ../dev/%.c
	$(CROSS_CC) $(CROSS_CCOPTS) $(CROSS_CC_SEGDISC) -c $<

clean:
	rm -f $(OBJS) $(JUNK)  core *~ bootrom.ihx bootrom.bin diskboot.bin fuzix.com fuzix.rom

image:
	../cpm-loader/makecpmloader ../cpm-loader/cpmload.bin ../fuzix.bin 0x88 fuzix.com
	../cpm-loader/makecpmloader ../cpm-loader/fuzixload.bin ../fuzix.bin 0x88 fuzix

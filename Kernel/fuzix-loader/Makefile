all:	makefuzixloader fuzixload.bin

makefuzixloader: makefuzixloader.c

fuzixload.bin:	fuzixload.s
	$(CROSS_AS) $(ASOPTS) fuzixload.s
	sdldz80 -nmi fuzixload.rel
	makebin -p fuzixload.ihx > fuzixload.bin

clean:
	rm -f *~ *.rst *.lst *.asm *.bin *.sym *.rel *.map *.ihx makefuzixloader

Write the .bin file to an SD card or CompactFlash card. The .bin file is a disk
image so it should be written to the raw disk, not as a file on a filesystem on
the disk. Any disk 128MB or larger will work fine.

Under Linux and Mac OS X you can use dd for this. For example if your SD card
was "/dev/sdc" you would do:

  dd bs=512 if=disk-n8vem-mark4.bin of=/dev/sdc

You may need to use sudo ("sudo dd ...")

Under Windows, try http://www.chrysocome.net/dd

The disk image contains two partitions. The first partition is a Fuzix root
file system.  The second partition contains eight CP/M slices which are largely
empty, save for the first slice which contains the Fuzix kernel ("FUZIX.COM")
for booting from CP/M.

The boot track of the disk also contains a copy of the kernel and an UNA BIOS
compatible bootstrap program.

There are two ways to boot the system;

1. At the UNA BIOS "Boot UNA unit number or ROM?" prompt, enter the unit number
of the disk

2. Boot into UNA CP/M and run FUZIX.COM

At the "bootdev:" prompt you should name the partition containing the root
filesystem, typically this will be "hda1" in a single disk system.

Will Sowerbutts, 2015-05-09
will@sowerbutts.com

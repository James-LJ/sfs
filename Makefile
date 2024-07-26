obj-m += sfs.o
sfs-objs := fs.o super.o inode.o file.o dir.o

KDIR ?= /lib/modules/$(shell uname -r)/build

MKFS = mkfs.sfs

all: $(MKFS)
	make -C $(KDIR) M=$(PWD) modules

IMAGE ?= test.img
IMAGESIZE ?= 50

$(MKFS): mkfs.c
	$(CC) -Wall -o $@ $<

$(IMAGE): $(MKFS)
	dd if=/dev/zero of=${IMAGE} bs=1M count=${IMAGESIZE}
	./$< $(IMAGE)

clean:
	make -C $(KDIR) M=$(PWD) clean
	rm -f *~ $(PWD)/*.ur-safe
	rm -f $(MKFS) $(IMAGE)

.PHONY: all clean

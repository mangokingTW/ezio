ARCH = x86_64
LINUX = linux
INITRD = initrd.img
PROG = ezio
STATIC_PROG = static-$(PROG)
VERSION = $(shell uname -r)

BUSYBOX_URL = https://www.busybox.net/downloads/binaries/1.28.1-defconfig-multiarch/busybox-x86_64

TEMP_INITRD = initrd.img.tmp
TEMP_DIR = tmp
TEMP = $(TEMP_DIR) $(TEMP_INITRD)

.PHONY: all
all: $(LINUX) $(INITRD)

$(LINUX):
	#cp /boot/vmlinuz-$(LINUX) $(LINUX)
	#cp linux $(LINUX)

$(INITRD):
	mkdir $(TEMP_DIR)
	cd $(TEMP_DIR) && cat ../archiso.img | cpio -id
	
# mv busybox
	#mv $(TEMP_DIR)/bin/busybox $(TEMP_DIR)/bin/busybox-alter
# download new busybox
	wget -q $(BUSYBOX_URL) -O $(TEMP_DIR)/bin/busybox
	cp /usr/bin/busybox $(TEMP_DIR)/bin/busybox
	chmod +x $(TEMP_DIR)/bin/busybox

# TODO maybe we need `install` rather than `mv`
	cp init $(TEMP_DIR)/init
	cp /usr/bin/sgdisk $(TEMP_DIR)/bin
	cp /usr/lib/libpopt.so.0 $(TEMP_DIR)/lib
	#cp inittab $(TEMP_DIR)/etc/
	cp udhcpc.sh $(TEMP_DIR)/bin/
	cp startup.sh $(TEMP_DIR)/bin/
	cp ../$(STATIC_PROG) $(TEMP_DIR)/bin/

# re-pack initrd.img
	cd $(TEMP_DIR) && find . -print0 | cpio --null -o --format=newc | gzip -9 > ../$(INITRD)

.PHONY: temp_clean
temp_clean:
	rm -rf $(TEMP)

.PHONY: clean
clean: temp_clean
	rm -rf $(LINUX) $(INITRD)

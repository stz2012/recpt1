TARGET := pt1_drv.ko
VERBOSITY = 0
EXTRA_CFLAGS += -Wformat=2
KVER ?= `uname -r`

KBUILD = /lib/modules/$(KVER)/build
INSTALL_DIR = /lib/modules/$(KVER)/kernel/drivers/video

all: ${TARGET}

pt1_drv.ko: pt1_pci.c pt1_i2c.c pt1_tuner.c pt1_tuner_data.c version.h
	make -C $(KBUILD) M=`pwd` V=$(VERBOSITY) modules

clean:
	make -C $(KBUILD) M=`pwd` V=$(VERBOSITY) clean

obj-m := pt1_drv.o

pt1_drv-objs := pt1_pci.o pt1_i2c.o pt1_tuner.o pt1_tuner_data.o

clean-files := *.o *.ko *.mod.[co] *~ version.h

version.h:
	eval `sed -e "s/\[0\]//" ./dkms.conf`; \
	GREV=`git rev-list HEAD | wc -l 2> /dev/null`; \
	if [ $$GREV != 0 ] ; then \
		printf "#define DRV_VERSION \"$${PACKAGE_VERSION}rev$$GREV\"\n#define DRV_RELDATE \"`git show --date=short --format=%ad | sed -n '1p' 2> /dev/null`\"\n#define DRV_NAME \"$${BUILT_MODULE_NAME}\"\n" > $@; \
	else \
		printf "#define DRV_VERSION \"$${PACKAGE_VERSION}\"\n#define DRV_RELDATE \"$$PACKAGE_RELDATE\"\n#define DRV_NAME \"$${BUILT_MODULE_NAME}\"\n" > $@; \
	fi

uninstall:
	rm -vf $(INSTALL_DIR)/$(TARGET)* /etc/udev/rules.d/99-pt1.rules

dkms: $(TARGET)
	if [ -d /etc/udev/rules.d -a ! -f /etc/udev/rules.d/99-pt1.rules ] ; then \
		install -m 644 etc/99-pt1.rules /etc/udev/rules.d ; \
	fi

install: uninstall dkms
	install -d $(INSTALL_DIR)
	install -m 644 $(TARGET) $(INSTALL_DIR)
	depmod -a $(KVER)

install_compress: install
	. $(KBUILD)/.config ; \
	if [ $$CONFIG_DECOMPRESS_XZ = "y" ] ; then \
		xz   -9e $(INSTALL_DIR)/$(TARGET); \
	elif [ $$CONFIG_DECOMPRESS_BZIP2 = "y" ] ; then \
		bzip2 -9 $(INSTALL_DIR)/$(TARGET); \
	elif [ $$CONFIG_DECOMPRESS_GZIP = "y" ] ; then \
		gzip  -9 $(INSTALL_DIR)/$(TARGET); \
	fi
	depmod -a $(KVER)

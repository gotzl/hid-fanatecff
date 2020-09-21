KVERSION := `uname -r`
KDIR := /lib/modules/${KVERSION}/build

default:
	$(MAKE) -C $(KDIR) M=$$PWD

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean


install:
	cp hid-ftec.ko /lib/modules/${KVERSION}/misc/ && depmod -a

uninstall:
	rm /lib/modules/${KVERSION}/misc/hid-ftec.ko && depmod -a

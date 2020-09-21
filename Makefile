KVERSION := `uname -r`
KDIR := /lib/modules/${KVERSION}/build

default:
	$(MAKE) -C $(KDIR) M=$$PWD

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean

install:
	cp hid-fanatec.ko /lib/modules/${KVERSION}/misc/ && depmod -a
	cp fanatec.rules /etc/udev/rules.d/99-fanatec.rules

uninstall:
	rm /lib/modules/${KVERSION}/misc/hid-fanatec.ko && depmod -a
	rm /etc/udev/rules.d/99-fanatec.rules

KVERSION := `uname -r`
KDIR := /lib/modules/${KVERSION}/build

ifeq ($(shell lsb_release -a 2> /dev/null | grep Distributor | awk '{ print $$3 }'),Ubuntu)
	INSTALLDIR := /lib/modules/${KVERSION}/kernel/drivers/misc/
else
	INSTALLDIR := /lib/modules/${KVERSION}/misc/
endif

default:
	$(MAKE) -C $(KDIR) M=$$PWD

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean

install:
	cp hid-fanatec.ko ${INSTALLDIR} && depmod -a
	cp fanatec.rules /etc/udev/rules.d/99-fanatec.rules

uninstall:
	rm ${INSTALLDIR} && depmod -a
	rm /etc/udev/rules.d/99-fanatec.rules

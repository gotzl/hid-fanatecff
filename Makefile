KVERSION ?= `uname -r`
KDIR ?= /lib/modules/${KVERSION}/build
MODULEDIR ?= /lib/modules/${KVERSION}/kernel/drivers/hid

default:
	@echo -e "\n::\033[32m Compiling Fanatec kernel module\033[0m"
	@echo "========================================"
	$(MAKE) -C $(KDIR) M=$$PWD

clean:
	@echo -e "\n::\033[32m Cleaning Fanatec kernel module\033[0m"
	@echo "========================================"
	$(MAKE) -C $(KDIR) M=$$PWD clean

install:
	@echo -e "\n::\033[34m Installing Fanatec kernel module/udev rule\033[0m"
	@echo "====================================================="
	@cp -v hid-fanatec.ko ${MODULEDIR}
	@cp -v fanatec.rules /etc/udev/rules.d/99-fanatec.rules
	depmod

uninstall:
	@echo -e "\n::\033[34m Uninstalling Fanatec kernel module/udev rule\033[0m"
	@echo "====================================================="
	@rm -fv ${MODULEDIR}/hid-fanatec.ko
	@rm -fv /etc/udev/rules.d/99-fanatec.rules
	depmod

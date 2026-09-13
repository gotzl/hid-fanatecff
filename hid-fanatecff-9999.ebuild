EAPI=8

inherit git-r3 linux-mod-r1 udev

DESCRIPTION="A kernel module for Fanatec HID devices"
HOMEPAGE="https://github.com/gotzl/hid-fanatecff"
EGIT_REPO_URI="https://github.com/gotzl/hid-fanatecff"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~amd64"

CONFIG_CHECK="~HID ~HID_GENERIC ~USB_HID ~HIDRAW ~UHID"

src_compile() {
    local modlist=(
        hid-fanatec=kernel/drivers/hid::
    )

    linux-mod-r1_src_compile
}

src_install() {
	linux-mod-r1_src_install

	udev_dorules fanatec.rules
}

pkg_postinst() {
	linux-mod-r1_pkg_postinst
	udev_reload
}

pkg_postrm() {
	udev_reload
}

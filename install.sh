#!/usr/bin/env sh

set -eu

if [ "$(id -u)" -ne 0 ]; then
    echo 'This script must be run as root!' >&2
    exit 1
fi

if ! [ -x "$(command -v dkms)" ]; then
    echo 'This script requires DKMS!' >&2
    exit 1
fi

if [ -n "$(dkms status hid-fanatec)" ]; then
    echo 'Driver is already installed!' >&2
    exit 1
fi

if [ -n "${SUDO_USER:-}" ]; then
    # Run as normal user to prevent "unsafe repository" error
    version=$(sudo -u "$SUDO_USER" git describe --tags 2> /dev/null || echo unknown)
else
    version=unknown
fi

source="/usr/src/hid-fanatec-$version"
log="/var/lib/dkms/hid-fanatec/$version/build/make.log"

echo "Installing hid-fanatec $version..."
cp -r . "$source"
cp -v fanatec.rules /etc/udev/rules.d/99-fanatec.rules
find "$source" -type f \( -name dkms.conf -o -name '*.c' \) -exec sed -i "s/#VERSION#/$version/" {} +

if [ "${1:-}" != --release ]; then
    echo 'ccflags-y += -DDEBUG' >> "$source/Kbuild"
fi

if ! dkms install -m hid-fanatec -v "$version"; then
    if [ -r "$log" ]; then
        cat "$log" >&2
    fi
    exit 1
fi

echo "installing module for other kernel versions"
ls /boot/initrd.img-* | cut -d- -f2- | xargs -n1 /usr/lib/dkms/dkms_autoinstaller start

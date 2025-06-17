#!/usr/bin/env sh

set -eu

if [ "$(id -u)" -ne 0 ]; then
    echo 'This script must be run as root!' >&2
    exit 1
fi

modules=$(lsmod | grep hid_fanatec || true)
version=$(dkms status hid-fanatec | head -n 1 | tr -s ',:/' ' ' | cut -d ' ' -f 2)

if [ -n "$modules" ]; then
    echo "Unloading modules: $modules..."
    # shellcheck disable=SC2086
    modprobe -r -a $modules
fi

if [ -n "$version" ]; then
    echo "Uninstalling hid-fanatec $version..."
    dkms remove -m hid-fanatec -v "$version" --all
    rm -r "/usr/src/hid-fanatec-$version"
else
    echo 'Driver is not installed!' >&2
fi

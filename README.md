A linux kernel driver that aims to add support for the FANATEC CSL Elite Wheel Base, including PS4 edition.

### Notes
The Wheel Base should be set to 'PC mode' for the driver to be selected (red LED). Known device IDs are
* 0EB7:0E03 FANATEC CSL Elite Wheel Base
* 0EB7:0005 FANATEC CSL Elite Wheel Base PS4
* 0EB7:6204 FANATEC CSL Elite Pedals
* (experimental: 0EB7:0001 FANATEC ClubSport Wheel Base V2)
* (experimental: 0EB7:0004 FANATEC ClubSport Wheel Base V2.5)

## Installation
Compile and install the driver

```
make
sudo make install
```

This installs the kernel module `hid-fanatec.ko` in the `misc` dir of the running kernel and puts `fanatec.rules` into `/etc/udev/rules.d`. These rules allows access to the device for `users` group and sets deadzone/fuzz to 0 so that any wheel input is detected immediately.
The driver should get loaded automatically when the wheel is plugged.

## Status
- Some support for `FF_CONSTANT`, implemented somehow analogous to `hid-lgff`. This seems to be enough to get rudimentary force-feedback support in most games.
- Some support for wheel LEDs: LEDs are accessable via sysfs
- Some support for wheel display: a sysfs file called `display` is created where a number can be written to that will be displayed. Negative value turns display off.
- some support CSL Elite pedals: loadcell adjustment via sysfs `load` (no readback yet)

See also the [hid-fanatecff-tools](https://github.com/gotzl/hid-fanatecff-tools) project which aims to add LED/Display functionallity for games.

## Planned
- support more effects
- support wheelbase LEDs

## Disclaimer
I take absolutly *no* responsibility for any malfunction of this driver and their consequences. If your device breaks or your hands get ripped off I'm sorry, though ;)

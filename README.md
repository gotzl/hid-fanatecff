A linux kernel driver that aims to add support for the FANATEC CSL Elite Wheel Base.

## Installation
No proper installation routine so far...

Put `fanatec.rules` into `/etc/udev/rules.d`. This rules allows access to the device for `users` group and sets deadzone/fuzz to 0 so that any wheel input is detected immediately.

Compile and inser the driver
```
make
insmod hid-fanatecff.ko
```
In case the `insmod` doesn't work because of unknown symbols, do `modprobe ff_memless` and try again.

## Status
Some support for `FF_CONSTANT`, implemented somehow analogous to `hid-lgff`. This seems to be enough to get rudimentary force-feedback support in most games.

## Planned
- support more effects
- support wheelbase LEDs
- support wheel LEDs
- support wheel display
- maybe support CSL Elite pedals (eg loadcell adjustment)
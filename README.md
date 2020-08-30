A linux kernel driver that aims to add support for the FANATEC CSL Elite Wheel Base.

## Installation
No proper installation routine so far...

Put `fanatec.rules` into `/etc/udev/rules.d`. This rules allows access to the device for `users` group and sets deadzone/fuzz to 0 so that any wheel input is detected immediately.

Compile and insert the driver
```
make
insmod hid-fanatecff.ko
```
In case the `insmod` doesn't work because of unknown symbols, do `modprobe ff_memless` and try again.

## Status
- Some support for `FF_CONSTANT`, implemented somehow analogous to `hid-lgff`. This seems to be enough to get rudimentary force-feedback support in most games.
- Some support for wheel LEDs: LEDs are accessable via sysfs
- Some support for wheel display: a sysfs file called `display` is created where a number can be written to that will be displayed. Negative value turns display off.
- To show speed/rpm from games, there is `fanatec_led_server.py`. So far, only AssettoCorsa and F1 2020 are supported via UDP telemetry data.

## Planned
- support more effects
- support wheelbase LEDs
- maybe support CSL Elite pedals (eg loadcell adjustment)

## Disclaime
I take absolutly *no* responsibility for any malfunction of this driver and their consequences. If your device breaks or your hands get ripped off I'm sorry, though ;)

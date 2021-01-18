A linux kernel driver that aims to add support for FANATEC devices.

### Known devices
The Wheel Base should be set to 'PC mode' for the driver to be selected (CSL Elite: red LED)
* 0EB7:0E03 FANATEC CSL Elite Wheel Base
* 0EB7:0005 FANATEC CSL Elite Wheel Base PS4
* 0EB7:6204 FANATEC CSL Elite Pedals
* (experimental: 0EB7:0001 FANATEC ClubSport Wheel Base V2)
* (experimental: 0EB7:0004 FANATEC ClubSport Wheel Base V2.5)
* (experimental: 0EB7:0006 Podium Wheel Base DD1)

## Installation
Compile and install the driver

```
make
sudo make install
```

This installs the kernel module `hid-fanatec.ko` in the `hid` dir of the running kernel and puts `fanatec.rules` into `/etc/udev/rules.d`. These rules allows access to the device for `users` group and sets deadzone/fuzz to 0 so that any wheel input is detected immediately.
The driver should get loaded automatically when the wheel is plugged.

## Status
### General
Support for `FF_CONSTANT`, implemented somehow analogous to `hid-lgff`. This seems to be enough to get rudimentary force-feedback support in most games.


### Device specific
Advanced functions of wheels/bases are available via sysfs. Base sysfs path:
`/sys/module/hid_fanatec/drivers/hid:ftec_csl_elite/0003:0EB7:0005.*/`

#### CSL Elite Base:
- RPM LEDs: `leds/0003:0EB7:0005.*::RPMx/brightness` (x from 1 to 9)
- tuning menu: 
    - get/set tuning menu slot: echo number into `SLOT`
    - values get/set: `BLI DPR DRI FEI FF FOR SEN SHO SPR`
    - reset all tuning sets by echoing anything into `RESET`

#### CSL Elite pedals: 
- loadcell adjustment: `load` (no readback yet)

#### ClubSport Forumla1 wheel:
- RPM LEDs (combined with base)
- display: `display` (negative value turns display off)


To access advanced functions from user space please see the [hid-fanatecff-tools](https://github.com/gotzl/hid-fanatecff-tools) project which also aims to support LED/Display access from games.

## Planned
- support more effects
- support more devices / advances functions of devices

## Disclaimer
I take absolutly *no* responsibility for any malfunction of this driver and their consequences. If your device breaks or your hands get ripped off I'm sorry, though ;)

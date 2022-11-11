# Linux kernel driver that aims to add support for FANATEC devices

## Known devices

The Wheel Base should be set to 'PC mode' for the driver to be selected (CSL Elite and CSL DD: red LED)

* 0EB7:0E03 FANATEC CSL Elite Wheel Base
* 0EB7:0005 FANATEC CSL Elite Wheel Base PS4
* 0EB7:6204 FANATEC CSL Elite Pedals
* (experimental: 0EB7:0001 FANATEC ClubSport Wheel Base V2)
* (experimental: 0EB7:0004 FANATEC ClubSport Wheel Base V2.5)
* (experimental: 0EB7:0006 Podium Wheel Base DD1)
* (experimental: 0EB7:0007 Podium Wheel Base DD2)
* (experimental: 0EB7:0011 CSR Elite/Forza Motorsport Wheel Base)
* (experimental: 0EB7:0020 CSL DD Wheel Base)

## Installation

Compile and install the driver

```sh
make
sudo make install
```

This installs the kernel module `hid-fanatec.ko` in the `hid` dir of the running kernel and puts `fanatec.rules` into `/etc/udev/rules.d`. These rules allows access to the device for `users` group and sets deadzone/fuzz to 0 so that any wheel input is detected immediately.
The driver should get loaded automatically when the wheel is plugged.

### Packaging

If you don't want to compile and install manually, following is a list of known packaged distributions.

| System | Package |
| ------ | ------- |
| AUR | [`hid-fanatecff-dkms`](https://aur.archlinux.org/packages/hid-fanatecff-dkms) |

## Status

### General

Support for a bunch of effects, mostly copy-pasted from [new-lg4ff](https://github.com/berarma/new-lg4ff). _Note:_ only tested with CSL Elite Wheelbase.
Currently not supported effects: FF_FRICTION, FF_INERTIA

### FFB in specific Games

Games I test (more or less regularly) and that are expected to work:

* AC / ACC
* Dirt2
* rFactor2

Games that don't work properly:

* F1 202X (#22)
* BeamNG.drive (#23)

### Device specific

Advanced functions of wheels/bases are available via sysfs. Base sysfs path:

`/sys/module/hid_fanatec/drivers/hid:ftec_csl_elite/0003:0EB7:0005.*/`

#### Common

* set/get range: echo number in degrees to `range`
* get id of mounted wheel: `wheel_id`

#### CSL Elite Base

* RPM LEDs: `leds/0003:0EB7:0005.*::RPMx/brightness` (x from 1 to 9)
* tuning menu:
  * get/set tuning menu slot: echo number into `SLOT`
  * values get/set: `BLI DPR DRI FEI FF FOR SEN SHO SPR`
  * reset all tuning sets by echoing anything into `RESET`

#### CSL Elite pedals

* loadcell adjustment: `load` (no readback yet)

#### ClubSport Forumla1 wheel

* RPM LEDs (combined with base)
* display: `display` (negative value turns display off)

#### ClubSport Pedals V3

* pedal vibration: `rumble`
  * 16776960 -> both pedals should rumble
  * 16711680 -> throttle pedal rumble
  * 65280 -> break pedal rumble
  * 0 -> stop rumble

To access advanced functions from user space please see the [hid-fanatecff-tools](https://github.com/gotzl/hid-fanatecff-tools) project which also aims to support LED/Display access from games.

## Planned

* support more effects
* support more devices / advances functions of devices

## Disclaimer

I take absolutly _no_ responsibility for any malfunction of this driver and their consequences. If your device breaks or your hands get ripped off I'm sorry, though. ;)

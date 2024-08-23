# Linux kernel driver that aims to add support for FANATEC devices

## Known devices

The Wheel Base should be set to 'PC mode' for the driver to be selected (CSL Elite and CSL DD: red LED)

* 0EB7:0E03 FANATEC CSL Elite Wheel Base
* 0EB7:0005 FANATEC CSL Elite Wheel Base PS4
* 0EB7:0020 FANATEC CSL DD / DD Pro / ClubSport DD Wheel Base
* 0EB7:6204 FANATEC CSL Elite Pedals
* (experimental: 0EB7:0001 FANATEC ClubSport Wheel Base V2)
* (experimental: 0EB7:0004 FANATEC ClubSport Wheel Base V2.5)
* (experimental: 0EB7:183b FANATEC ClubSport Pedals V3)
* (experimental: 0EB7:0006 Podium Wheel Base DD1)
* (experimental: 0EB7:0007 Podium Wheel Base DD2)
* (experimental: 0EB7:0011 CSR Elite/Forza Motorsport Wheel Base)

## Installation

### Dependencies
This is a `out-of-tree` kernel module. Building it depends on
* compiler (gcc)
* make
* kernel-headers

Examples of installing kernel-headers for some distros:   
Ubuntu: `sudo apt install linux-headers-generic` or `sudo apt install linux-headers-$(uname -r)`   
Fedora: `sudo dnf install kernel-devel`   
Arch: `pacman -S linux-headers`   

### Compile and install the driver

```sh
make
sudo make install
```

Reload the new udev rules, depending on the Linux distribution, without rebooting:

```sh
sudo udevadm control --reload-rules && sudo udevadm trigger
```

This installs the kernel module `hid-fanatec.ko` in the `hid` dir of the running kernel and puts `fanatec.rules` into `/etc/udev/rules.d`. These rules allows access to the device for `games` group and sets deadzone/fuzz to 0 so that any wheel input is detected immediately.
The driver should get loaded automatically when the wheel is plugged.

### Packaging

If you don't want to compile and install manually, following is a list of known packaged distributions.

| System | Package |
| ------ | ------- |
| AUR | [`hid-fanatecff-dkms`](https://aur.archlinux.org/packages/hid-fanatecff-dkms) |

## Status

### General

Support for a bunch of effects, mostly copy-pasted from [new-lg4ff](https://github.com/berarma/new-lg4ff).  
Currently, FF_FRICTION and FF_INERTIA effects have experimental support in this driver.

**Note:** With Proton 7/8, in some games the wheel is not detected properly when starting it for the first time (ie, when a new Proton-prefix is created). The current workaround is to first start the game with Proton 6, and then switch to a later one. (See also #67)

### FFB in specific Games

Games that are expected to work (tested by me and others more or less regularly):

* AC / ACC (*)
* Automobilista 2
* DiRT 4
* DiRT Rally 2 / WRC (**)
* F1 22/23 (***)
* rFactor2
* Rennsport
* RRRE

Games that don't work properly:

* F1 2020/2021 (#22)
* BeamNG.drive (Proton) (#23)


(* input devices can get mixed-up in ACC; best have only the wheel-base connected and always use the same USB-slot)   
(** uses experimental FF_FRICTION effect)   
(*** unsure if all effects are present)   

### Device specific

Advanced functions of wheels/bases are available via sysfs. Generally, these files should be writable by users in the `games` group. Base sysfs path:

`/sys/module/hid_fanatec/drivers/hid:fanatec/0003:0EB7:<PID>.*/`


#### Common

* Set/get range: echo number in degrees to `range`
* Get id of mounted wheel: `wheel_id`
* Tuning menu (experimental): `tuning_menu/*` 
  * Get/set 'standard'/'advanced' mode: `andvanced_mode`
  * Get/set tuning menu slot: echo number into `SLOT`
  * Values get/set: `BLI DPR DRI FEI FF FOR SEN SHO SPR ...` (files depend on wheel-base)
  * Reset all tuning sets by echoing anything into `RESET`

#### CSL Elite Base

* RPM LEDs: `/sys/class/leds/0003:0EB7:0005.*::RPMx/brightness` (x from 1 to 9)

#### ClubSport Forumla1 wheel

* RPM LEDs (combined with base)
* Display: `display` (negative value turns display off)

#### CSL Elite pedals

* Loadcell adjustment: `load` (no readback yet)

#### ClubSport Pedals V3

* pedal vibration: `rumble`
  * 0xFFFF00 -> both pedals should rumble
  * 0xFF0000 -> throttle pedal rumble
  * 0xFF00 -> break pedal rumble
  * 0 -> stop rumble

To access advanced functions from user space please see the [hid-fanatecff-tools](https://github.com/gotzl/hid-fanatecff-tools) project which also aims to support LED/Display access from games.

## Planned

* Support more effects
* Support more devices / advances functions of devices
* Support different wheels-rims and their quirks
* Packaging for more distros

## Contact

If you have an issue, feature request or a general question, feel free to open a ticket on GitHub.

## Disclaimer

I am not associated with Endor AG/Fanatec. 
I take absolutely _no_ responsibility for any malfunction of this driver and their consequences. If your device breaks or your hands get ripped off I'm sorry, though. ;)

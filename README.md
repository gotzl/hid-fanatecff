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

### Implementation  

#### Integration with Linux Kernel Subsystems  

This driver implements a [Linux force-feedback (FF) driver](https://www.kernel.org/doc/html/latest/input/ff.html), allowing force-feedback effects to be uploaded via the standard API. These effects are translated into a custom HID protocol and sent to the device asynchronously, using a timer that defaults to 2ms.  

Supported are a bunch of effects, the code is largely copy-pasted/adapted from [new-lg4ff](https://github.com/berarma/new-lg4ff).  
Currently, FF_FRICTION and FF_INERTIA effects have experimental support in this driver.

Additionally, the driver integrates with the [Linux LED interface](https://www.kernel.org/doc/html/latest/leds/leds-class.html), enabling control of the RPM and other LEDs found on most Fanatec wheel rims. This is achieved by writing to the appropriate `sysfs` files. Further details on these and other `sysfs` files exposed by the driver can be found in the [Device-Specific Section](#device-specific).  

#### Integration with Wine/Proton  

Wine/Proton provides multiple methods for accessing HID devices. Typically, it interfaces with the Linux input subsystem either directly or through SDL, using this information to create a corresponding Windows input device. While this allows games to utilize HID and force-feedback functionality, it does not support LEDs or other advanced features.  

Notably, the Fanatec SDK—used by certain games—often encounters issues when interacting with the Windows input device created in this manner.  

As an alternative, Wine/Proton can use [HIDRAW](https://docs.kernel.org/hid/hidraw.html) to create Windows input devices directly from a device’s HID descriptor. This approach allows Wine/Proton to communicate with the device as if running in a native Windows environment, enabling proper interaction with the Fanatec SDK for LED and display control.  

For force feedback to function correctly in Wine/Proton using HIDRAW, the HID descriptor must expose [HID PID](https://www.usb.org/document-library/device-class-definition-pid-10-0) functionality. To achieve this, the driver extends the device's HID descriptor with the necessary HID PID components and exposes them through the HIDRAW interface. HID PID commands from Wine/Proton are intercepted, translated into the custom HID protocol, and sent to the device. All other communication is directly passed through.  

By default, HIDRAW is not enabled in Wine. To enable it, see the [EnableHidraw registry key](https://gitlab.winehq.org/wine/wine/-/wikis/Useful-Registry-Keys).  

The Proton wine fork maintains a hardcoded list of devices for which HIDRAW is enabled. Beginning with Proton ?, HIDRAW will be enabled for Fanatec wheel bases. Prior versions of Proton will fall back to the Linux input/SDL method.  


### FFB in specific Games

Games that are expected to work (tested by me and others more or less regularly):

* AC / ACC (*) / ACE
* Automobilista 2
* DiRT 4
* DiRT Rally 2 / WRC (**)
* F1 22/23 (***)
* rFactor2
* Rennsport
* RRRE
* Wreckfest

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

* RPM LEDs: `leds/0003:0EB7:0005.*::RPMx/brightness` (x from 1 to 9)

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

## Troubleshooting
### No FFB, nothing on LEDs/display
Check permissions `ls -l /dev/hidrawXX`, if it is not `0666`, then check with `udevadm test /dev/hidrawXX` if there are any additional rules overwriting the mode set by the `fanatec.rules` file.
Check correct driver module version is loaded: `modinfo hid-fanatec | grep hidraw`.
Check game log `PROTON_LOG=1 WINEDEBUG=+hid,+input,+dinput %command%`, ensure that there is a line called `found 3 TLCs`. If it is not there, then a proton/wine version is used that doesn't support multi TLCs (yet).

### Game hangs/crashes at startup
* Clear enumerated HID devices: `protontricks -c "wine reg delete 'HKLM\System\CurrentControlSet\Enum\HID' /f" <appid>`
* If using separated pedals, 'pump' a pedal (to generate input) during game startup (seen in F1 23, AMS2, ??)

### Large deadzone or chunky input
Check the deadzone/flatness/fuzz settings:
```
evdev-joystick -s /dev/input/by-id/usb-Fanatec*-event-joystick
```
This should output s.t. like `... flatness: 0 (=0.00%), fuzz: 0)` for all axis.
If this is not the case then check that the udev rule works. Execute
```
udevadm test /dev/input/by-id/usb-Fanatec*-event-joystick
```
and see if `/etc/udev/rules.d/99-fanatec.rules` gets called.

### Dirt Rally 2: no FFB with CSL DD / ClubSport DD
Edit `input/devices/device_defines.xml` and add a line like this
```
<device id="{00200EB7-0000-0000-0000-504944564944}" name="ftec_csl_dd_pc" priority="100" type="wheel" />
```
Adjust the `00200EB7` to your PID (first 4 chars).

### ACC: wrong controller mapping (ie pedals show up as wheel input and vice-versa)
Try to delete `My Documents/Assetto Corsa Competizione/Config/controls.json`   
Note: You'll probably have to re-map all your Buttons!

## Contact

If you have an issue, feature request or a general question, feel free to open a ticket on GitHub.

## Disclaimer

I am not associated with Endor AG/Fanatec. 
I take absolutely _no_ responsibility for any malfunction of this driver and their consequences. If your device breaks or your hands get ripped off I'm sorry, though. ;)

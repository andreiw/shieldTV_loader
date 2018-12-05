nVidia Shield TV fastboot bootloader.
=====================================

Last updated December 5th, 2018.

The Shield TV is based on the 64-bit nVidia X1 chip, and allows performing an
unlock via "fastboot oem unlock", allowing custom OS images to be booted.

It's all very tedious, though. You have to press hold the (only) touch
button on the case for about 9 secs after applying power. You have to
use `fastboot flash`, as `fastboot boot` is kinda broken on newer firmware.
Then you have to `fastboot reboot`.

We can do better than that!

# Purpose

A fastboot implementation to make development less painful. You can flash it
to your boot partition, and on every power cycle it will patiently wait for
you to send another image to run... or... do other things to it.

It allows booting images over USB or exploring the system. It does not
support flashing images. The regular NV fastboot implementation already
does that.

# Requirements

* A Shield TV, unlocked. Search Youtube for walkthroughs.
* Shield TV firmware version >= 1.3.
* GNU Make.
* An AArch64 GNU toolchain.
* ADB/fastboot tools.
* Bootimg tools (https://github.com/pbatard/bootimg-tools), built and somewhere in your path.
* An HDMI-capable screen, capable of 1920x1080.

# How to build

1. `$ CROSS_COMPILE=aarch64-linux-gnu- make` should give you `shieldTV_loader`.

2. After getting to the NV fastboot menu (either via `adb reboot-bootloader` or with the manual method), use `fastboot flash boot shieldTV_loader` to replace your boot partition.

**NOTE: Until you reflash the Android boot partition, there's no more Shield OS.**

3. Power cycle (or `fastboot reboot`).

# Commands

- `flash run` will boot a binary or bootimg-wrapped binary image of your choice. If using mkbootimg-wrapped images, make sure `pagesize` corresponds to actual image alignment. Also, your image will be loaded at the first opportune place, so it better be position-independent.

```
$ fastboot flash run your_binary_image
$ fastboot flash run your_mkbootimg_wrapped_binary_image
```

- `oem peek` is a pretty useful hexdumper.

```
$ fastboot oem peek <addr> <access> <optional: items>
$ fastboot oem peek 0x1000 8 8

(bootloader) 00000000a00002e9 0000000000100008 0000000000100010
(bootloader) 0000000000100018 0000000000000000 0000000000000000
(bootloader) 0000000000000000 0000000000000000
```

- `oem poke` is pretty straightforward, too.

```
$ fastboot oem poke 0x80070000 c hello there
$ fastboot oem peek 0x80070000 c 16

(bootloader)  h  e  l  l  o 20  t  h  e  r  e 00 04 00 00 00

$ fastboot oem poke 0x80070000 4 1 2 3 4
$ fastboot oem peek 0x80070000 4 4

(bootloader) 00000001 00000002 00000003 00000004
```

- `oem echo` prints to the video screen with that sweet Sun OpenBoot font.

```
$ fastboot oem echo hello there
```

- `oem alloc` allocates a chunk of memory given parameters.

```
$ fastboot oem alloc <size> <align>
```

- 'oem alloc32' allocates a chunk of memory below 4GB.

```
$ fastboot oem alloc32 <size> <align>
```

- 'oem free' frees a previous allocated chunk.

```
$ fastboot oem alloc32 0x100 4096

(bootloader) 0xfd1fd000

$ fastboot oem free 0xfd1fd000 0x100 4096

$ fastboot oem alloc32 0x100 4096

(bootloader) 0xfd1fd000
```

- `oem smccc` runs arbitrary ARM SMC commands, following the ARM SMCCC, so you can see how terrible the PSCI implementation really is. Maximum 8 parameters. Returns the four potential return values.

```
$ fastboot oem smccc 0

(bootloader) 0xdeadbeef 0x0 0x0 0x0
```

- `oem reboot` resets the Shield. Argument can be one of "normal", "bootloader", "rcm", "recovery" or a PMC SCRATCH0 value if you're feeling brave. The regular reboot commands are also supported.

```
$ fastboot reboot
$ fastboot reboot-bootloader
$ fastboot oem reboot bootloader
```

# Contact

Andrey Warkentin <andrey.warkentin@gmail.com>

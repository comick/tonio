Tonio is free juke-box software for children and beyond
=======================================================

Inspired by commercial success of _tonies®_, Tonio is the limitless free software alternative for makers with a legacy.

The project consists of Tonio daemon software, implementing Jukebox functionality, and a set of Buildroot external configurations for selected boards.


Key features
============

Tonio turns RFID tags into keys for playing audio playlists.

It supports anything that `libvlc` can play, streaming included.

- Tonio is free software released under the terms of GNU General Public License 3
- Tonio is _creative_ by default
- Tonio is DRM-free
- Tonio boots in few seconds, courtesy of Buildroot

Super simple human interface design:

- lay an RFID tag on RFID sensitive area -> play
- move tag away from sensitive area -> pause
- buttons for next/previous track
- buttons for volume up/down


Getting started
===============

Supported hardware
------------------

- Raspberry PI 3
- RC522 RFID
- Push buttons for volume and track control (optional)


Wiring things together
----------------------

Push buttons wiring is configured in `tonio.h` header.
Here are the default settings, wiring pi pin numbering:

```
#PIN_PREV 1
#define PIN_NEXT 4

#define PIN_VOL_DOWN 29
#define PIN_VOL_UP 5
```

Tonio daemon assumes SPI interface to RC522, connect accordingly.

Reference assembly coming soon! Or maybe not.


Building the root file system
-----------------------------

Download Buildroot into some folder, say `$HOME/buildroot-2019.02.07`.
Version 2019.02.07 was used so far. The project may move to subsequent LTS versions of build root when avaibale.

Clone the project in your favorite folder, say `$HOME/tonio`.

Setup wireless. Assumption is made that your wireless network is WPA2.
Find out `wpa_supplicant.conf.sample` in desired board `rootfs_overlay/etc/init.d`, change things and copy to `wpa_supplicant.conf`.
Same for `rootfs_overlay/etc/network/interfaces`. Copy `rootfs_overlay/etc/network/interfaces` and change essid according to your reuirements.

> Access point functionality can be offered for initial geek-free setup. Not done yet, see TODO for contributing.

```
$ cd $HOME/buildroot-2019.02.07
$ make BR2_EXTERNAL=$HOME/tonio tonio_pi3_defconfig
$ make
```

wait. Buildroot is building the root image from the ground up, it will take some time. Kind half an hour or so.

When you see your command prompt again, and few lines like:

```
...
INFO: hdimage(sdcard.img): adding partition 'boot' (in MBR) from 'boot.vfat' ...
INFO: hdimage(sdcard.img): adding partition 'rootfs' (in MBR) from 'rootfs.ext4' ...
INFO: hdimage(sdcard.img): writing MBR
```

that means build is done.

Writing image to SD card
------------------------

Root image built successfully and you SD card image is ready.
First find out your SD card device file, say /dev/sdX, then:

```$ sudo dd if output/sdcard.img of=/dev/sdX && sync```

Plug you SD card into the board and switch on the device. First boot will take care of expaning root filesystem to the entire extend of your SD card and restart once.

Congratulations, you're done! Time to setup playlists and bind them to RFID tags.


Playlists setup
===============

A web interface for playlist management has been started, albeit far from complete. See TODO and feel free to contribute to speed up things.

When you place a tag, Tonio daemon looks up `/usr/share/tonio` for presence of a folder named like the hex string coresponding to detected tag UUID.
If folder is there, say `ABCDEF01`, the file `/usr/share/tonio/ABCDEF01/ABCDEF01.m3u` will play. That's your entry point.
File format is the kinda-standard `m3u`, anything uderstood by vlc can be put there. Hint: web radios also work.

> The Buildroot configuration may miss some decoders, only the most commonly used are enabled.
> Should you find some quite common decoder, please feel free to contribute improved buildroot config.


Finding out your tag UUID is possible via Tonio daemon log. Dropbear is listening, ssh into your device and `tail -f /var/log/messages`.
Put your tag on the sensor and see log trace. After that scp is your friend.

Once a playlist is loaded on the device, put the appropriate tag in the sensitive area, your tracks should play.


Support and feature requests
============================

Nope. But you can create tickets for the wider audience or fix issues yourself and contribute.


Contributing
============

You're wellcome. Merge requests are the way to go.
Just in case you're out of ideas:

- [ ] Start as Access Point for initial network cofiguration
- [ ] Web interface for ordinary people to setup playlists
- [ ] Rewrite with Ada and SPARK, children stuff can't (absolutely can't!) fail at runtime
- [ ] Move to `pigpio`, maybe abstract gpio layer to support more boards
- [ ] Supporting/testing more boards (eg: pi zero)
- [ ] Strip kernel from unused modules and move to static device table to faster build and boot


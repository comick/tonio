Tonio is the free juke-box software for children and beyond
===========================================================

Inspired by commercial success of _tonies®_, Tonio is the limitless free software alternative for makers with a legacy.

The project consists of Tonio daemon software, implementing Jukebox functionality, and a set of carefully crafted Buildroot external configurations for selected boards.


Key features
============

Tonio turns RFID tags into keys for playing audio playlists.

- Tonio supports anything that `libvlc` can play, streaming included.
- Tonio is free software released under the terms of GNU General Public License 3
- Tonio is _creative_ by default
- Tonio is DRM-free
- Tonio boots in few seconds, courtesy of Buildroot

Super simple human interface design:

- lay an RFID tag on RFID sensitive area -> play
- move tag away from sensitive area -> pause
- buttons for next/previous track
- buttons for volume up/down

Getting started from source
===========================

Pre-built images are available for supported boards. Take a look to the [wiki Quick Start page](https://github.com/comick/tonio/wiki/Quick-Start).

Still, you may want to build the root image yourself. Supporting a new board or adding more codecs than the default are all good reasons for doing so.

Supported hardware
------------------

- Boards
  - Raspberry Pi 3
  - Raspberry Pi 0 Wireless (fat kernel, untested audio)
- RC522 RFID
- Push buttons via libgpiod for volume and track control (optional)


Wiring things together
----------------------

All aspects of `tonio` daemon are configured via `/etc/tonio.conf`, using `libconfuse` format.
Linux chardev GPIO pin numbers apply. For debugging convenience `libgpiod` tools are made available as a tonio dependency.

Tonio daemon assumes SPI interface to RC522. GPIO switch and spid device can be configured via `/etc/tonio.conf`.

[Reference assembly documentation is under progress in the wiki](https://github.com/comick/tonio/wiki/Reference-Assembly).


Building the root file system
-----------------------------

[Download Buildroot](https://buildroot.org/download.html) and extract into some folder, say `$HOME/buildroot-2021.02.2`.
Version 2021.02.2 is currently used. The project tries to keep current with latest LTS version of Buildroot.

Clone this project in your favorite folder, say `$HOME/tonio`.

```
$ cd $HOME/buildroot-2021.02.2
$ make BR2_EXTERNAL=$HOME/tonio tonio_raspberrypi3_defconfig
$ make
```

> HINT: `raspberry3` is used in this example, but multiple boards are supported, see the `board` folder for the list of those.

Time to wait... Buildroot is building both toolchain and root image from the ground up, it will take some time. Kind half an hour or more.

After a while you should see your command prompt again, preceeded by few lines like:

```
...
INFO: hdimage(sdcard.img): adding partition 'boot' (in MBR) from 'boot.vfat' ...
INFO: hdimage(sdcard.img): adding partition 'rootfs' (in MBR) from 'rootfs.ext4' ...
INFO: hdimage(sdcard.img): adding partition 'library' (in MBR) from 'library.vfat' ...
INFO: hdimage(sdcard.img): writing MBR
```

That means your build is done.

Writing image to an SD card
---------------------------

Root image was built successfully and you SD card image is ready.
First find out your SD card device file, say /dev/sdX, then:

```$ sudo dd if output/sdcard.img of=/dev/sdX && sync```

Plug you SD card into the board and switch the device on.

Access Tonio via network
------------------------

Your tonio is up and running and new wireless network should be around: its name is `tonio` and default password `toniocartonio`. Connect to it.

Once associated, access your Tonio web interface at http://tonio.local.
The web interface allows to check Tonio status, read the log and manage playlists.

SSH access is also enabled by default `$ ssh root@tonio.local`. Default password is `tonio`.

> HINT: before building, you may want to add your pub key to `$TONIO_SRC/board/$BOARD_NAME/rootfs_overlay/root/.ssh/authorized_keys` for faster SSH access.

Congratulations, you're done! Time to setup playlists and bind them to RFID tags.


Playlists setup
===============

Open the web interface status section at [http://tonio.local/#status](http://tonio.local/#status).
Now move an unused RFID tag on the sensitive area and wait the status page to shows you the unique tag id, say `${MY_TAG_ID}`.

Until playlist management from web UI is complete, some more work is required.

Media files and playlist are meant to be copied on the `vfat` partition named `TONIO` that Tonio mounts at `/mnt/media/`.
When you place a tag, Tonio daemon looks for the folder `/mnt/media/library/${MY_TAG_ID}`.
If folder is there, the first file with extension `.m3u`, found in folder `/usr/share/tonio/library/${MY_TAG_ID}` will be played. That's your entry point.
File format is the kinda-standard `m3u`, anything uderstood by vlc can be put there. Make sure both `m3u` and media files are uploaded and __relative__ paths in `m3u` are correct.

> Hint: web radios also work. Just make sure your tonio is connected to the Internet.

Currently, the web UI only allows you to see the content of playlists. Later, playlist management could be done without requiring the user to unplug the SD card.

> The Buildroot configuration has a very limit set of decoders, only the most commonly used (vorbis, mp3) are enabled.
> If you need more decoding capabilities, make sure decoder packages are enabled before build.


Support and feature requests
============================

Nope. But you can create tickets for the wider audience or fix issues yourself and contribute.
Sure enough, you can also say thanks and show your Tonio at work!


Contributing
============

You're wellcome. Pull requests are the way to go.
Just in case you're out of ideas:

- [ ] Supporting/testing more boards (eg: pi zero, orange pi, ...)
- [ ] First time setup page
- [ ] Web UI needs is mostly read only
- [ ] Finalize web interface for ordinary people to setup playlists

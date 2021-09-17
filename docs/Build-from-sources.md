Building the root file system
-----------------------------

[Download Buildroot](https://buildroot.org/download.html) and extract into some folder, say `$HOME/buildroot-2021.02.4`.
Version 2021.02.4 is currently used. The project tries to keep current with latest LTS version of Buildroot.

Clone this project in your favorite folder, say `$HOME/tonio`.

```
$ cd $HOME/buildroot-2021.02.4
$ make BR2_EXTERNAL=$HOME/tonio tonio_raspberrypi3_defconfig
$ make
```

> HINT: while `raspberry3` is used in this example, other boards might be supported. See the `board` folder for the full list.

Time to wait... Buildroot is building both toolchain and root image from the ground up, it will take some time. Kind half an hour or more.

After a while you should see your command prompt again, preceeded by few lines resembling this:

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

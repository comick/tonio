A step by step guide to go from BOM to a working Tonio using from pre-built images.

# Bring the Hardware together
You can use one of our [Reference Assemblies](https://github.com/comick/tonio/wiki/Reference-Assembly) for supported boards.

# SD card setup

Releases are tagged upon build success. The build produces SD images for supported boards.

Go to [Releases](https://github.com/comick/tonio/releases) and download the SD card image for your board.

Now flash and SD card with `$ sudo dd if=sdcard-configname.img of=/dev/sdX && sync`, where `X` is the letter identifying you SD card block device.

> NOTE: The SD image is provided with almost zero-sized data partition, and shall be expanded to make room for your tracks and playlists.

Here we'll use `fatresize` command (a piece of Parted tool):

1. Get the maximum size of the fat partition by `$ sudo fatresize --info /dev/sdX3` (the data partition is the third one) and take note of the `Max size`:

```fatresize 1.0.2 (07/03/08)
FAT: fat32
Size: 3874106368
Min size: 3638191104
Max size: 4095737344
```

**TODO: put this into a script. Something like `$ ./scripts/flash.sh /dev/sdX` which takes car of everything descript here.**

2. Resize the partition with `$ sudo fatresize --size "$MAX_SIZE" /dev/sdX3`

Command line Parted or one of its GUIs can be used as well.

# Boot you brand new Tonio

Plug the SD card into your board and switch it on: after a few seconds, the wireless network `tonio` should be around.

Connect to it using the default password `toniocartonio` and point your browser to [http://tonio.local](http://tonio.local) (this may not work until https://github.com/comick/tonio/issues/3 is done).

Follow the instructions for first time setup (not available until https://github.com/comick/tonio/issues/4 is done) and enjoy Tonio!
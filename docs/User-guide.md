Wiring things together
----------------------

All aspects of `tonio` daemon are configured via `/etc/tonio.conf`, using `libconfuse` format.
Linux chardev GPIO pin numbers apply. For debugging convenience `libgpiod` tools are made available as a tonio dependency.

Tonio daemon assumes SPI interface to RC522. GPIO switch and spid device can be configured via `/etc/tonio.conf`.

[Reference assembly documentation is under progress in the wiki](https://github.com/comick/tonio/wiki/Reference-Assembly).


Access Tonio via network
------------------------

Your tonio is up and running and a new wireless network should be around: its name is `tonio` and default password `toniocartonio`. Connect to it.

Once associated, access your Tonio web interface at [http://tonio.local].
The web interface allows to check Tonio status, read the log and manage playlists.

SSH access is also enabled by default `$ ssh root@tonio.local`. Default password is `tonio`.

> HINT: before building, you may want to add your pub key to `$TONIO_SRC/board/$BOARD_NAME/rootfs_overlay/root/.ssh/authorized_keys` for faster SSH access.

Congratulations, you're done! Time to setup playlists and bind them to RFID tags.


Playlists setup
===============

Open the web interface status section at [http://tonio.local/#status](http://tonio.local/#status).
Now move an unused RFID tag on the sensitive area and wait the status page to show you the unique tag id (a 8 digits hex string), say `${MY_TAG_ID}`.

Until playlist management from web UI is complete, some more work is required.

Media files and playlist are meant to be copied on the `vfat` partition named `TONIO` that Tonio mounts at `/mnt/media/`.
When you place a tag, Tonio daemon looks for the folder `/mnt/media/library/${MY_TAG_ID}`.
If folder is there, the first file with extension `.m3u`, found in folder `/usr/share/tonio/library/${MY_TAG_ID}` will be played. That's your playlist entry point.
File format is the kinda-standard `m3u`, anything uderstood by vlc can be put there. Make sure both `m3u` and media files are uploaded and __relative__ paths in `m3u` are correct.

> HINT: web radios also work. Just make sure your tonio is connected to the Internet.

Currently, the web UI only allows you to see the content of playlists. Later, playlist management could be done without requiring the user to unplug the SD card.

> The Buildroot configuration has a very limit set of decoders, only the most commonly used (vorbis, mp3) are enabled.
> If you need more decoding capabilities, make sure decoder packages are enabled before build.

Tonio is the free juke-box software for children and beyond
===========================================================

Inspired by commercial success of _tonies®_, Tonio is the limitless free software children juke-box for makers with a legacy.

The project consists of Tonio daemon software, implementing Jukebox functionality, and a set of carefully crafted Buildroot external configurations for selected boards.

Key features
============

Tonio turns RFID tags into keys for playing audio playlists.

- Tonio supports anything that `libvlc` can play, streaming included
- Tonio is free software released under the terms of GNU General Public License 3
- Tonio is _creative_ by default
- Tonio is DRM-free
- Tonio boots in few seconds, courtesy of Buildroot

Super simple human interface design:

- lay an RFID tag on RFID sensitive area -> play
- move tag away from sensitive area -> pause
- buttons for next/previous track
- buttons for volume up/down

Supported hardware
------------------

- Boards
  - Raspberry Pi 3
- RC522 RFID
- Push buttons via `libgpiod` for volume and track control (optional)


Getting started
===============

Pre-built images are available for supported boards. Take a look to the wiki [quick Start page](https://github.com/comick/tonio/wiki/Quick-Start).

Still, you may want to [build the root image](https://github.com/comick/tonio/wiki/Build-from-sources) yourself. Supporting a new board or adding more codecs than the default are all good reasons for doing so.


Support and feature requests
============================

Nope. But you can create tickets for the wider audience or fix issues yourself and contribute.
Sure enough, you can also say thanks and show your Tonio at work!

Contributing
============

You're wellcome. Pull requests are the way to go.
Just in case you're out of ideas look at the board for version One dot Zero at https://github.com/comick/tonio/projects/2.

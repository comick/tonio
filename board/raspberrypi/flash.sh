#!/usr/bin/env bash

IMG_PATH=$1
DEV=$2

ls $DEV?* -1 | xargs umount || true

sudo dd if=$IMG_PATH of=$DEV
sync

umount ${DEV}3 || true

sudo parted ${DEV} unit B resizepart 3 $(( `sudo fatresize -i ${DEV}3 | grep -Po '(?<=Max size: )[0-9]+(?=$)'` - 1 ))

sudo fatresize -s $(( `sudo fatresize -i ${DEV}3 | grep -Po '(?<=Max size: )[0-9]+(?=$)'` - 1 )) ${DEV}3

echo Done
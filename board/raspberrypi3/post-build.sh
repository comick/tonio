#!/bin/sh

set -u
set -e

# Add a console on tty1
if [ -e ${TARGET_DIR}/etc/inittab ]; then
    grep -qE '^tty1::' ${TARGET_DIR}/etc/inittab || \
	sed -i '/GENERIC_SERIAL/a\
tty1::respawn:/sbin/getty -L  tty1 0 vt100 # HDMI console' ${TARGET_DIR}/etc/inittab
fi

# Automount media folder
cat << __EOF__ >> "${TARGET_DIR}/etc/inittab"
::sysinit:/bin/mkdir -p /mnt/media                                                  │
::sysinit:/bin/mount -t vfat /dev/mmcblk0p3 /mnt/media
__EOF__

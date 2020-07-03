#!/bin/sh

set -u
set -e

# Add a console on tty1
if [ -e ${TARGET_DIR}/etc/inittab ]; then
    grep -qE '^tty1::' ${TARGET_DIR}/etc/inittab || \
	sed -i '/GENERIC_SERIAL/a\
tty1::respawn:/sbin/getty -L  tty1 0 vt100 # HDMI console' ${TARGET_DIR}/etc/inittab
fi

# Automount tonio media folder
if [ -e ${TARGET_DIR}/etc/inittab ]; then
    grep -qE '^# mount tonio media folder' ${TARGET_DIR}/etc/inittab || \
	sed -i '/now run any rc scripts/i\
# mount tonio media folder\
::sysinit:/bin/mkdir -p /mnt/media\
::sysinit:/bin/mount -t vfat /dev/mmcblk0p3 /mnt/media' ${TARGET_DIR}/etc/inittab
fi
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

# Disable avahi on ap0
if [ -e ${TARGET_DIR}/etc/avahi/avahi-daemon.conf ]; then
    grep -qE '^deny-interfaces=ap0' ${TARGET_DIR}/etc/avahi/avahi-daemon.conf || \
	sed -i '/\[server\]/a\
deny-interfaces=ap0' ${TARGET_DIR}/etc/avahi/avahi-daemon.conf
fi

BOARD_DIR="$(dirname $0)"

# Use tonio own factory interfaces config if not overridden
if [ ! -e ${BOARD_DIR}/rootfs_overlay/etc/network/interfaces ]; then
    cp ${TARGET_DIR}/etc/network/interfaces.sample ${TARGET_DIR}/etc/network/interfaces
fi

# Use tonio own factory WPA config if not overridden
if [ ! -e ${BOARD_DIR}/rootfs_overlay/etc/wpa_supplicant.conf ]; then
    cp ${TARGET_DIR}/etc/wpa_supplicant.conf.sample ${TARGET_DIR}/etc/wpa_supplicant.conf
fi

# Use tonio own factory tonio config if not overridden
if [ ! -e ${BOARD_DIR}/rootfs_overlay/etc/tonio.conf ]; then
    cp ${TARGET_DIR}/etc/tonio.conf.sample ${TARGET_DIR}/etc/tonio.conf
fi
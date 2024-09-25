#!/usr/bin/env bash

ROOTFS_PATH=$1
PART=$2
DEV=$3
ROOT_PART=${DEV}${PART}

echo "Umounting ${ROOT_PART}."
umount ${ROOT_PART} || true

echo "Fashing $ROOTFS_PATH to ${ROOT_PART}"
sudo dd if=$ROOTFS_PATH of=${ROOT_PART} bs=1M
sync

echo Done.

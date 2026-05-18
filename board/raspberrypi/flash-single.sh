#!/usr/bin/env bash

ROOTFS_PATH=$1
PART=$2
DEV=$3
ROOT_PART=${DEV}${PART}

echo "Unmounting ${ROOT_PART}."
umount "${ROOT_PART}" || true

echo "Flashing $ROOTFS_PATH to ${ROOT_PART}"
sudo dd if="$ROOTFS_PATH" of="${ROOT_PART}" bs=1M status=progress
sync

echo Done.

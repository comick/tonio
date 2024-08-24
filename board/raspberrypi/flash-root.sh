#!/usr/bin/env bash

ROOTFS_PATH=$1
DEV=$2
ROOT_PART=${DEV}2

echo "Umounting ${ROOT_PART}."
umount ${ROOT_PART} || true

echo "Fashing $ROOTFS_PATH to ${ROOT_PART}"
sudo dd if=$ROOTFS_PATH of=${ROOT_PART} bs=1M
sync

echo Done.

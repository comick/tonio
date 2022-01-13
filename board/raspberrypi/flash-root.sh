#!/usr/bin/env bash

ROOTFS_PATH=$1
DEV=$2

umount ${DEV}2 || true

sudo dd if=$ROOTFS_PATH of=${DEV}2
sync

echo Done
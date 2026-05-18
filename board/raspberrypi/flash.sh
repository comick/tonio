#!/usr/bin/env bash
set -e

IMG_PATH=$1
DEV=$2

echo "Unmounting $DEV and all its partitions."
ls "$DEV"?* -1 | xargs umount || true

echo "Flashing $IMG_PATH to $DEV."
sudo dd if="$IMG_PATH" of="$DEV" bs=1M status=progress
sync

echo "Inflating last partition to fill entire SD space left."

# Unmount the last partition
sudo umount "${DEV}3" || true

# Resize the last partition to fill the entire disk
sudo parted -s "$DEV" resizepart "3" 100%
sync

# Wait for kernel to re-read the updated partition table
sudo partprobe "${DEV}" || sudo blockdev --rereadpt "${DEV}"

# Resize fat file system
sudo fatresize -s max "${DEV}${LAST_PART}"

echo "Done."

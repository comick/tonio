#!/bin/bash

set -e

BOARD_DIR="$(dirname $0)"
BOARD_NAME="$(basename ${BOARD_DIR})"
GENIMAGE_CFG="${BOARD_DIR}/genimage-${BOARD_NAME}.cfg"
GENIMAGE_TMP="${BUILD_DIR}/genimage.tmp"

for arg in "$@"
do
	case "${arg}" in
		--add-miniuart-bt-overlay)
		if ! grep -qE '^dtoverlay=' "${BINARIES_DIR}/rpi-firmware/config.txt"; then
			echo "Adding 'dtoverlay=miniuart-bt' to config.txt (fixes ttyAMA0 serial console)."
			cat << __EOF__ >> "${BINARIES_DIR}/rpi-firmware/config.txt"

# fixes rpi (3B, 3B+, 3A+, 4B and Zero W) ttyAMA0 serial console
dtoverlay=miniuart-bt
__EOF__
		fi
		;;
		--aarch64)
		# Run a 64bits kernel (armv8)
		sed -e '/^kernel=/s,=.*,=Image,' -i "${BINARIES_DIR}/rpi-firmware/config.txt"
		if ! grep -qE '^arm_64bit=1' "${BINARIES_DIR}/rpi-firmware/config.txt"; then
			cat << __EOF__ >> "${BINARIES_DIR}/rpi-firmware/config.txt"

# enable 64bits support
arm_64bit=1
__EOF__
		fi
		;;
		--gpu_mem_256=*|--gpu_mem_512=*|--gpu_mem_1024=*)
		# Set GPU memory
		gpu_mem="${arg:2}"
		sed -e "/^${gpu_mem%=*}=/s,=.*,=${gpu_mem##*=}," -i "${BINARIES_DIR}/rpi-firmware/config.txt"
		;;
	esac

done

# enables audio out
if ! grep -qE '^dtparam=audio=' "${BINARIES_DIR}/rpi-firmware/config.txt"; then 
    echo "Adding 'dtparam=audio=on' to config.txt (enabled audio output)." 
    cat << __EOF__ >> "${BINARIES_DIR}/rpi-firmware/config.txt" 
dtparam=audio=on
__EOF__
fi

# enables spi for rfid
if ! grep -qE '^dtparam=spi=' "${BINARIES_DIR}/rpi-firmware/config.txt"; then 
    echo "Adding 'dtparam=spi=' to config.txt (enabled spi interface)." 
    cat << __EOF__ >> "${BINARIES_DIR}/rpi-firmware/config.txt" 
dtparam=spi=on
__EOF__
fi

# enable audio on gpio, required eg on rpi zer where there is no audio out.
#dtoverlay=pwm-2chan,pin=18,func=2,pin2=13,func2=4


# Pass an empty rootpath. genimage makes a full copy of the given rootpath to
# ${GENIMAGE_TMP}/root so passing TARGET_DIR would be a waste of time and disk
# space. We don't rely on genimage to build the rootfs image, just to insert a
# pre-built one in the disk image.

trap 'rm -rf "${ROOTPATH_TMP}"' EXIT
ROOTPATH_TMP="$(mktemp -d)"

rm -rf "${GENIMAGE_TMP}"

ln -sf "${BR2_EXTERNAL_TONIO_PATH}/library" "${ROOTPATH_TMP}/library"

# Empty library gives place to zero-size vfat partition.
# Forcing minimum size in such case.
LIBRARY_SIZE=`du "${ROOTPATH_TMP}/library" -cbL | tail -n 1 | cut -f 1`
MIN_FAT32_SIZE=34091520
if (( $LIBRARY_SIZE < $MIN_FAT32_SIZE )); then
    export LIBRARY_VFAT_SIZE="$MIN_FAT32_SIZE"
fi

genimage \
	--rootpath "${ROOTPATH_TMP}"   \
	--tmppath "${GENIMAGE_TMP}"    \
	--inputpath "${BINARIES_DIR}"  \
	--outputpath "${BINARIES_DIR}" \
	--config "${GENIMAGE_CFG}"

exit $?

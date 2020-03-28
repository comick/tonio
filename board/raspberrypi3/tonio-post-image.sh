#!/bin/bash

set -e


if ! grep -qE '^dtparam=audio=' "${BINARIES_DIR}/rpi-firmware/config.txt"; then 
    echo "Adding 'dtparam=audio=on' to config.txt (enabled audio output)." 
    cat << __EOF__ >> "${BINARIES_DIR}/rpi-firmware/config.txt" 
# Enables audio out
dtparam=audio=on
__EOF__
fi


if ! grep -qE '^dtparam=spi=' "${BINARIES_DIR}/rpi-firmware/config.txt"; then 
    echo "Adding 'dtparam=spi=' to config.txt (enabled spi interface)." 
    cat << __EOF__ >> "${BINARIES_DIR}/rpi-firmware/config.txt" 
# Enables spi for rfid
dtparam=spi=on
__EOF__
fi

# enable audio on gpio, required eg on rpi zer where there is no audio out.
#dtoverlay=pwm-2chan,pin=18,func=2,pin2=13,func2=4

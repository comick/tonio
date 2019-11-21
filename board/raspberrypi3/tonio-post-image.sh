#!/bin/bash

set -e

cat << __EOF__ >> "${BINARIES_DIR}/rpi-firmware/config.txt"

# enable audio
dtparam=audio=on

# enable spidev
dtparam=spi=on

# enable audio on gpio
#dtoverlay=pwm-2chan,pin=18,func=2,pin2=13,func2=4
__EOF__


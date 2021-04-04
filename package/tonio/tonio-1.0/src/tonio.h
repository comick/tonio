/**
 * Copyright (c) 2020 Michele Comignano <comick@gmail.com>
 * This file is part of Tonio.
 *
 * Tonio is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Tonio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Tonio.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TONIO_H
#define TONIO_H

#include <stdio.h>
#include <errno.h>
#include <syslog.h>

#define CFG_MEDIA_ROOT "media-root"
#define CFG_MIXER_CARD "mixer-card"
#define CFG_MIXER_SELEM "mixer-selem"
#define CFG_VOLUME_MAX "volume-max"
#define CFG_BTN_TRACK_PREVIOUS "btn-track-previous"
#define CFG_BTN_TRACK_NEXT "btn-track-next"
#define CFG_BTN_VOLUME_DOWN "btn-volume-down"
#define CFG_BTN_VOLUME_UP "btn-volume-up"
#define CFG_MFRC522_SWITCH "mfrc522-switch"
#define CFG_MFRC522_SPI_DEV "mfrc522-spi-dev"

#define I_CHECK(V, A) do { \
    int ret = (V); \
    if (ret < 0) { \
        syslog(LOG_ERR, "%s at %s (%d): %s", #V, __FILE__, __LINE__, strerror(errno)); \
        A; \
    } \
} while (0);

#define P_CHECK(V, A) do { \
    if ((V) == NULL) { \
        syslog(LOG_ERR, "%s at %s (%d): %s", #V, __FILE__, __LINE__, strerror(errno)); \
        A; \
    } \
} while (0);

#endif /* TONIO_H */


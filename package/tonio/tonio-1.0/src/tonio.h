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

#define MEDIA_FOLDER "/usr/share/tonio"

#define PIN_PREV 1
#define PIN_NEXT 4

#define PIN_VOL_DOWN 29
#define PIN_VOL_UP 5

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


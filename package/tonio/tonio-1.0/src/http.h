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

#ifndef HTTP_H
#define HTTP_H

#include <microhttpd.h>
#include "media.h"

#define PORT 80

typedef struct tn_http tn_http_t;

tn_http_t *tn_http_init(tn_media_t *, uint8_t *, char *);
void tn_http_stop(tn_http_t *);

#endif /* HTTP_H */


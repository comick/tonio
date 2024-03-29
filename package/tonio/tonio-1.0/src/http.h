/**
 * Copyright (c) 2020-2023 Michele Comignano <mcdev@playlinux.net>
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
#include <confuse.h>

#include "media.h"

typedef struct tn_http tn_http_t;

tn_http_t *tn_http_init(tn_media_t *media, uint8_t *selected_card_id, cfg_t *cfg);
void tn_http_stop(tn_http_t *self);

#endif /* HTTP_H */


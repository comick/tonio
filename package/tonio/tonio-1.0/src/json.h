/**
 * Copyright (c) 2022, 2023 Michele Comignano <mcdev@playlinux.net>
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

#ifndef JSON_H
#define JSON_H

#define JSON_TRUE "true"
#define JSON_FALSE "false"

/** Token type, constants and factory functions. */

struct cj_token {
    uint32_t type;
    void *value;
};

typedef struct cj_token cj_token_t;

extern const cj_token_t cj_null;
extern const cj_token_t cj_array_push;
extern const cj_token_t cj_array_pop;

cj_token_t cj_string(char *buf);

/** Token streams and interfaces. */
typedef struct cj_token_stream cj_token_stream_t;

typedef cj_token_t (*cj_token_stream_next_t) (void *cls);
typedef void (*cj_token_stream_free_t) (void *cls);

cj_token_stream_t *cj_token_stream_new(void *cls , cj_token_stream_next_t nxt, cj_token_stream_free_t free);

void cj_token_stream_free(void *cls);

ssize_t cj_microhttpd_callback(void *cls, uint64_t pos, char *buf, size_t max);

#endif /* JSON_H */

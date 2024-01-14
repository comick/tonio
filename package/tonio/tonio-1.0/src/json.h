/**
 * Copyright (c) 2022, 2024 Michele Comignano <mcdev@playlinux.net>
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

/** Token type, constants and factory functions. */

typedef enum cj_token_type {
    CJ_NONE = 0,
    CJ_ARRAY_PUSH = 1 << 0,
    CJ_ARRAY_POP = 1 << 1,
    CJ_STRING = 1 << 2,
    CJ_NULL = 1 << 3,
    CJ_OBJECT_PUSH = 1 << 4,
    CJ_OBJECT_POP = 1 << 5,
    CJ_KEY = 1 << 6,
    CJ_NUMBER = 1 << 7,
    CJ_TRUE = 1 << 8,
    CJ_FALSE = 1 << 9
} cj_token_type_t;

typedef struct cj_token {
    cj_token_type_t type;

    union {
        const char *str;
        double number;
    } value;
} cj_token_t;

extern const cj_token_t cj_eos;
extern const cj_token_t cj_null;
extern const cj_token_t cj_true;
extern const cj_token_t cj_false;
extern const cj_token_t cj_array_push;
extern const cj_token_t cj_array_pop;
extern const cj_token_t cj_object_push;
extern const cj_token_t cj_object_pop;

cj_token_t cj_string(const char *buf);
cj_token_t cj_number(double n);
cj_token_t cj_key(const char *buf);

/** Token streams and interfaces. */
typedef struct cj_token_stream cj_token_stream_t;

typedef cj_token_t(*cj_token_stream_next_t) (void *cls);
typedef void (*cj_token_stream_free_t) (void *cls);

cj_token_stream_t *cj_token_stream_new(void *cls, cj_token_stream_next_t nxt, cj_token_stream_free_t free);

void cj_token_stream_free(void *cls);

ssize_t cj_microhttpd_callback(void *cls, uint64_t pos, char *buf, size_t max);

/** Utility things for simple static JSON structures. */
typedef struct cj_tokens_it {
    int current;
    int count;
    cj_token_t *tks;
} cj_tokens_it_t;

static cj_token_t cj_next_token(void *cls) {
    
    cj_tokens_it_t *cd = (cj_tokens_it_t *) cls;

    if (cd->current == cd->count) {
        return cj_eos;
    }

    int pos = cd->current;
    cd->current += 1;
    return cd->tks[pos];
}


#endif /* JSON_H */

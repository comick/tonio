/**
 * Copyright (c) 2022-2023 Michele Comignano <mcdev@playlinux.net>
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <microhttpd.h>

#include "json.h"

#define JSON_ARRAY_BEGIN "["
#define JSON_ARRAY_END "]"
#define JSON_QUOTE "\""
#define JSON_ITEM_SEP ","

#define CJ_ARRAY_PUSH 1
#define CJ_ARRAY_POP 2
#define CJ_STRING 4
#define CJ_NULL 8

struct cj_token_stream {
    void *cls; // context for next
    uint32_t last_token;
    uint32_t expected_tokens;
    cj_token_stream_next_t next;
    cj_token_stream_free_t free;
};

const cj_token_t cj_null = {CJ_NULL, NULL};
const cj_token_t cj_array_push = {CJ_ARRAY_PUSH, NULL};
const cj_token_t cj_array_pop = {CJ_ARRAY_POP, NULL};

cj_token_t cj_string(char *buf) {
    return (cj_token_t){ CJ_STRING, buf};
}

cj_token_stream_t *cj_token_stream_new(void * cls,
        cj_token_stream_next_t next,
        cj_token_stream_free_t free) {
    cj_token_stream_t *it = malloc(sizeof (cj_token_stream_t));
    it->cls = cls;
    it->last_token = 0;
    it->expected_tokens = CJ_ARRAY_PUSH;
    it->next = next;
    it->free = free;
    return it;
}

void cj_token_stream_free(void *cls) {
    cj_token_stream_t *it = (cj_token_stream_t *) cls;
    it->free(it->cls);
    free(it);
}

ssize_t cj_microhttpd_callback(void *cls, uint64_t pos, char *buf, size_t max) {
    cj_token_stream_t *ts = (cj_token_stream_t *) cls;

    if (ts->expected_tokens == 0) {
        return MHD_CONTENT_READER_END_OF_STREAM;
    }

    cj_token_t next = ts->next(ts->cls);

    uint32_t previous_token = ts->last_token;
    ts->last_token = next.type;

    char *val;
    switch (next.type) {
        case CJ_ARRAY_PUSH:
            memcpy(buf, JSON_ARRAY_BEGIN, strlen(JSON_ARRAY_BEGIN));
            ts->expected_tokens = CJ_STRING | CJ_ARRAY_POP;
            return strlen(JSON_ARRAY_BEGIN);
        case CJ_ARRAY_POP:
            ts->expected_tokens = 0;
            memcpy(buf, JSON_ARRAY_END, strlen(JSON_ARRAY_END));
            return strlen(JSON_ARRAY_END);
        case CJ_STRING:
            val = (char *) (next.value);

            int offset = 0;

            if (previous_token == CJ_STRING) {
                memcpy(buf + offset, JSON_ITEM_SEP, strlen(JSON_ITEM_SEP));
                offset += strlen(JSON_ITEM_SEP);
            }
            memcpy(buf + offset, JSON_QUOTE, strlen(JSON_QUOTE));
            offset += strlen(JSON_QUOTE);

            memcpy(buf + offset, val, strlen(val));
            offset += strlen(val);
            //free(next); // TODO free somewhere

            memcpy(buf + offset, JSON_QUOTE, strlen(JSON_QUOTE));
            offset += strlen(JSON_QUOTE);

            return offset;
        default:
            return MHD_CONTENT_READER_END_OF_STREAM;
    }
}

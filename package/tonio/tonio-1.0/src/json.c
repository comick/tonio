/**
 * Copyright (c) 2022-2024 Michele Comignano <mcdev@playlinux.net>
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

#define _CJ_VALUE_PUSH (CJ_ARRAY_PUSH | CJ_KEY | CJ_NULL | CJ_OBJECT_PUSH | CJ_TRUE | CJ_FALSE | CJ_STRING | CJ_NUMBER)
#define _CJ_VALUE_POP (CJ_ARRAY_POP | CJ_NULL | CJ_OBJECT_POP | CJ_TRUE | CJ_FALSE | CJ_STRING | CJ_NUMBER)

struct cj_token_stream {
    void *cls; // context for next
    cj_token_type_t last_token_type;
    uint32_t expected_tokens;
    cj_token_stream_next_t next;
    cj_token_stream_free_t free;
};

const cj_token_t cj_eos = {CJ_NONE, .value.str = NULL};
const cj_token_t cj_null = {CJ_NULL, .value.str = "null"};
const cj_token_t cj_true = {CJ_TRUE, .value.str = "true"};
const cj_token_t cj_false = {CJ_FALSE, .value.str = "false"};
const cj_token_t cj_array_push = {CJ_ARRAY_PUSH, .value.str = "["};
const cj_token_t cj_array_pop = {CJ_ARRAY_POP, .value.str = "]"};
const cj_token_t cj_object_push = {CJ_OBJECT_PUSH, .value.str = "{"};
const cj_token_t cj_object_pop = {CJ_OBJECT_POP, .value.str = "}"};

cj_token_t cj_string(char *buf) {
    if (buf == NULL) {
        return cj_null;
    }
    return (cj_token_t){ CJ_STRING, .value.str = buf};
}

cj_token_t cj_number(double n) {
    return (cj_token_t){ CJ_NUMBER, .value.number = n};
}

cj_token_t cj_key(char *buf) {
    if (buf == NULL) {
        return cj_null;
    }
    return (cj_token_t){ CJ_KEY, .value.str = buf};
}

cj_token_stream_t *cj_token_stream_new(void * cls,
        cj_token_stream_next_t next,
        cj_token_stream_free_t free) {
    cj_token_stream_t *it = malloc(sizeof (cj_token_stream_t));
    it->cls = cls;
    it->last_token_type = CJ_NONE;
    it->expected_tokens = _CJ_VALUE_POP;
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

    cj_token_t tk = ts->next(ts->cls);

    if (tk.type == CJ_NONE) {
        return MHD_CONTENT_READER_END_OF_STREAM;
    }


    int offset = 0;

    if ((tk.type & _CJ_VALUE_PUSH) && (ts->last_token_type & _CJ_VALUE_POP)) {
        *buf = ',';
        offset += 1;
    }

    ts->last_token_type = tk.type;

    switch (tk.type) {
        case CJ_ARRAY_PUSH:
            memcpy(buf + offset, tk.value.str, strlen(tk.value.str));
            return strlen(tk.value.str);
        case CJ_ARRAY_POP:
            ts->expected_tokens = 0;
            memcpy(buf + offset, tk.value.str, strlen(tk.value.str));
            return strlen(tk.value.str);
        case CJ_OBJECT_PUSH:
            memcpy(buf + offset, tk.value.str, strlen(tk.value.str));
            return strlen(tk.value.str);
        case CJ_OBJECT_POP:
            ts->expected_tokens = 0;
            memcpy(buf + offset, tk.value.str, strlen(tk.value.str));
            return strlen(tk.value.str);
        case CJ_NULL:
        case CJ_TRUE:
        case CJ_FALSE:
            memcpy(buf + offset, tk.value.str, strlen(tk.value.str));
            offset += strlen(tk.value.str);
            return offset;
        case CJ_NUMBER:
            offset += sprintf(buf + offset, "%g", tk.value.number);
            return offset;
        case CJ_KEY:
            *(buf + offset) = '"';
            offset += 1;

            memcpy(buf + offset, tk.value.str, strlen(tk.value.str));
            offset += strlen(tk.value.str);
            //free(next); // TODO free somewhere

            *(buf + offset) = '"';
            offset += 1;

            *(buf + offset) = ':';
            offset += 1;

            return offset;
        case CJ_STRING:
            *(buf + offset) = '"';
            offset += 1;

            memcpy(buf + offset, tk.value.str, strlen(tk.value.str));
            offset += strlen(tk.value.str);
            //free(next); // TODO free somewhere

            *(buf + offset) = '"';
            offset += 1;

            return offset;
        default:
            return MHD_CONTENT_READER_END_OF_STREAM;
    }
}

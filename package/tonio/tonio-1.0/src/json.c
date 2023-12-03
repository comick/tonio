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

struct tn_json_string_iterator {
    void *cls; // context for next
    bool done; // default false
    int pos; // default -1
    tn_json_string_iterator_next_t next;
    tn_json_string_iterator_free_t free;
};

tn_json_string_iterator_t *tn_json_string_iterator_new(void * cls,
        tn_json_string_iterator_next_t next,
        tn_json_string_iterator_free_t free) {
    tn_json_string_iterator_t *it = malloc(sizeof (tn_json_string_iterator_t));
    it->cls = cls;
    it->done = false;
    it->pos = -1;
    it->next = next;
    it->free = free;
    return it;
}

void tn_json_string_iterator_free(void *cls) {
    tn_json_string_iterator_t *it = (tn_json_string_iterator_t *) cls;
    it->free(it->cls);
    free(it);
}

ssize_t tn_json_string_array_callback(void *cls, uint64_t pos, char *buf, size_t max) {
    tn_json_string_iterator_t *it = (tn_json_string_iterator_t *) cls;

    if (it->done) {
        return MHD_CONTENT_READER_END_OF_STREAM;
    }

    if (it->pos == -1) {
        memcpy(buf, JSON_ARRAY_BEGIN, strlen(JSON_ARRAY_BEGIN));
        it->pos++;
        return strlen(JSON_ARRAY_BEGIN);
    }

    char *next = it->next(it->cls);
 
    if (next == NULL) {
        it->done = true;
        memcpy(buf, JSON_ARRAY_END, strlen(JSON_ARRAY_END));
        return strlen(JSON_ARRAY_END);
    } else {
        it->pos++;
    }

    int offset = 0;

    if (pos != strlen(JSON_ARRAY_BEGIN)) {
        memcpy(buf + offset, JSON_ITEM_SEP, strlen(JSON_ITEM_SEP));
        offset += strlen(JSON_ITEM_SEP);
    }
    memcpy(buf + offset, JSON_QUOTE, strlen(JSON_QUOTE));
    offset += strlen(JSON_QUOTE);

    memcpy(buf + offset, next, strlen(next));
    offset += strlen(next);
    //free(next); // TODO free somewhere

    memcpy(buf + offset, JSON_QUOTE, strlen(JSON_QUOTE));
    offset += strlen(JSON_QUOTE);

    return offset;
}

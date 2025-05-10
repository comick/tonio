/**
 * Copyright (c) 2022, 2025 Michele Comignano <mcdev@playlinux.net>
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


#define CJ_END_OF_STREAM ((ssize_t) -1)
#define CJ_END_WITH_ERROR ((ssize_t) -2)

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

#define _CJ_VALUE_PUSH (CJ_ARRAY_PUSH | CJ_KEY | CJ_NULL | CJ_OBJECT_PUSH | CJ_TRUE | CJ_FALSE | CJ_STRING | CJ_NUMBER)
#define _CJ_VALUE_POP (CJ_ARRAY_POP | CJ_NULL | CJ_OBJECT_POP | CJ_TRUE | CJ_FALSE | CJ_STRING | CJ_NUMBER)

typedef struct cj_token {
    cj_token_type_t type;

    union {
        struct {
            const char *buf;
            size_t len;
        };
        double number;
    } value;
} cj_token_t;

const cj_token_t cj_eos = {CJ_NONE, .value.buf = NULL};
const cj_token_t cj_null = {CJ_NULL, .value.len = 4, .value.buf = "null"};
const cj_token_t cj_true = {CJ_TRUE, .value.len = 4, .value.buf = "true"};
const cj_token_t cj_false = {CJ_FALSE, .value.len = 5, .value.buf = "false"};
const cj_token_t cj_array_push = {CJ_ARRAY_PUSH, .value.len = 1, .value.buf = "["};
const cj_token_t cj_array_pop = {CJ_ARRAY_POP, .value.len = 1, .value.buf = "]"};
const cj_token_t cj_object_push = {CJ_OBJECT_PUSH, .value.len = 1, .value.buf = "{"};
const cj_token_t cj_object_pop = {CJ_OBJECT_POP, .value.len = 1, .value.buf = "}"};


cj_token_t cj_string(size_t len, const char buf[len + 1]) {
    if (buf == NULL) {
        return cj_null;
    }
    return (cj_token_t){ CJ_STRING, .value.buf = buf, .value.len = len};
}

cj_token_t cj_number(double n) {
    return (cj_token_t){ CJ_NUMBER, .value.number = n};
}

cj_token_t cj_key(size_t len, const char buf[len + 1]) {
    if (buf == NULL) {
        return cj_null;
    }
    return (cj_token_t){ CJ_KEY, .value.buf = buf, .value.len = len};
}


/** Token streams and interfaces. */

// Callback for next element in token stream.
typedef cj_token_t(*cj_token_stream_next_t) (void *cls);
// Callback to free additional resources when tkoen stream is consumed.
typedef void (*cj_token_stream_free_t) (void *cls);

typedef struct cj_token_stream {
    void *cls; // context for next
    cj_token_type_t last_token_type;
    uint32_t expected_tokens;
    cj_token_stream_next_t next;
    cj_token_stream_free_t free;
} cj_token_stream_t;


// Creates a new token stream.
cj_token_stream_t *cj_token_stream_new(void * cls, cj_token_stream_next_t next, cj_token_stream_free_t free) {
    cj_token_stream_t *it = malloc(sizeof (cj_token_stream_t));
    it->cls = cls;
    it->last_token_type = CJ_NONE;
    it->expected_tokens = _CJ_VALUE_POP;
    it->next = next;
    it->free = free;
    return it;
}

/**
 * Callback to free additional resources associated to the stream.
 * 
 * @param cls pointer to additional stream resources.
 */
void cj_token_stream_free(void *cls) {
    cj_token_stream_t *it = (cj_token_stream_t *) cls;
    if (it->free != NULL) {
        it->free(it->cls);
    }
    free(it);
}

#define B_CHECK(OFFSET, MAX) do { \
    if ((OFFSET) > (MAX)) { \
        return CJ_END_WITH_ERROR; \
    } \
} while (0);

/**
 * Step function writing json to out buffer.
 * 
 * @param ts token stream.
 * @param pos
 * @param max max number of bytes the buffer can accomodate.
 * @param buf target buffer.
 * @return 
 */
 ssize_t cj_token_stream_writer(cj_token_stream_t *ts, size_t max, char buf[max]) {
    cj_token_t tk = ts->next(ts->cls);

    if (tk.type == CJ_NONE) {
        return CJ_END_OF_STREAM;
    }

    size_t offset = 0;

    if ((tk.type & _CJ_VALUE_PUSH) && (ts->last_token_type & _CJ_VALUE_POP)) {
        B_CHECK(offset + 1, max);
        buf[offset] = ',';
        offset += 1;
    }

    ts->last_token_type = tk.type;

    switch (tk.type) {
        case CJ_ARRAY_PUSH:
        case CJ_ARRAY_POP:
        case CJ_OBJECT_PUSH:
        case CJ_OBJECT_POP:
        case CJ_NULL:
        case CJ_TRUE:
        case CJ_FALSE:
            B_CHECK(offset + tk.value.len, max);
            memcpy(buf + offset, tk.value.buf, tk.value.len);
            offset += tk.value.len;
            return offset;
        case CJ_NUMBER:
            offset += snprintf(buf + offset, max, "%g", tk.value.number);
            // snprintf returns the bytes it would have written when truncated.
            // safe to check after if that was the case.
            B_CHECK(offset, max);
            return offset;
        case CJ_KEY:
        case CJ_STRING:
            B_CHECK(offset + tk.value.len + 3, max);

            buf[offset] = '"';
            offset += 1;

            
            memcpy(buf + offset, tk.value.buf, tk.value.len);
            offset += tk.value.len;

            buf[offset] = '"';
            offset += 1;

            if (tk.type == CJ_KEY) {
                buf[offset] = ':';
                offset += 1;
            }

            return offset;
        default:
            return CJ_END_WITH_ERROR;
    }
}

/** Utility things for simple static JSON structures. */
typedef struct cj_static_tokens_it {
    int current;
    int count;
    cj_token_t *tks;
} cj_static_tokens_it_t;

cj_static_tokens_it_t *cj_static_tokens_it_new(size_t count, cj_token_t tks[count]) {
    cj_static_tokens_it_t *it = malloc(sizeof(cj_static_tokens_it_t));
    it->count = count;
    it->current = 0;
    it->tks = tks;
    return it;
}

void cj_static_tokens_it_free(void *cls) {
    cj_static_tokens_it_t *cd = (cj_static_tokens_it_t *) cls;
    if (cd != NULL) {
        free(cd);
    }
}

cj_static_tokens_it_t cj_static_tokens_it_of(size_t count, cj_token_t tks[count]) {
    cj_static_tokens_it_t it;
    it.count = count;
    it.current = 0;
    it.tks = tks;
    return it;
}

cj_token_t cj_static_token_next(void *cls) {
    
    cj_static_tokens_it_t *cd = (cj_static_tokens_it_t *) cls;

    if (cd->current == cd->count) {
        return cj_eos;
    }

    int pos = cd->current;
    cd->current += 1;
    return cd->tks[pos];
}

#endif /* JSON_H */

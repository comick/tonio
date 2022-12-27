/**
 * Copyright (c) 2022 Michele Comignano <mcdev@playlinux.net>
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

typedef struct tn_json_string_iterator tn_json_string_iterator_t;

typedef char * (*tn_json_string_iterator_next_t) (void *cls);
typedef void (*tn_json_string_iterator_free_t) (void *cls);

tn_json_string_iterator_t *tn_json_string_iterator_new(void *, tn_json_string_iterator_next_t, tn_json_string_iterator_free_t);

void tn_json_string_iterator_free(void *);

ssize_t tn_json_string_array_callback(void *cls, uint64_t pos, char *buf, size_t max);

#endif /* JSON_H */


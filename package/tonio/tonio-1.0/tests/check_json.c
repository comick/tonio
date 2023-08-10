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

#include <check.h>
#include <microhttpd.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/json.h"

static char *_next_none(void *cls) {
    return NULL;
}

typedef struct _count_down {
    int current;
    int min;
} _count_down_t;

static char *_next_count_down(void *cls) {
    _count_down_t *cd = (_count_down_t *) cls;
    if (cd->current < cd->min) {
        return NULL;
    }

    int ret = cd->current;
    cd->current -= 1;

    char *str = malloc(10 * sizeof (char));
    sprintf(str, "%d", ret);
    return str;
}

static void _free_nothing(void *cls) {
}

static void _free_something(void *cls) {
    int *some_cls = (int *)cls;
    *some_cls = 'x';
}

START_TEST(test_free) {
    size_t buf_size = 1000;
    char *buf = (char *) malloc(sizeof (char) * buf_size);
    int some_cls = 'o';
    tn_json_string_iterator_t *it = tn_json_string_iterator_new(&some_cls, _next_none, _free_something);
    uint64_t pos = 0l;

    uint64_t n;
    while ((n = tn_json_string_array_callback(it, pos, buf, buf_size)) != MHD_CONTENT_READER_END_OF_STREAM) {
        pos += n;
    }
    *(buf + pos) = '\0';
    tn_json_string_iterator_free(it);
    
    ck_assert_str_eq(buf, "[]");
    ck_assert_int_eq(some_cls, 'x');

    free(buf);
}

END_TEST


START_TEST(test_empty_array) {
    size_t buf_size = 1000;
    char * buf = (char *) malloc(sizeof (char) * buf_size);
    tn_json_string_iterator_t *it = tn_json_string_iterator_new(NULL, _next_none, _free_nothing);
    uint64_t pos = 0l;

    uint64_t n;
    while ((n = tn_json_string_array_callback(it, pos, buf, buf_size)) != MHD_CONTENT_READER_END_OF_STREAM) {
        pos += n;
    }
    *(buf + pos) = '\0';
    tn_json_string_iterator_free(it);

    ck_assert_str_eq(buf, "[]");

    free(buf);
}

END_TEST


START_TEST(test_non_empty_array) {
    size_t buf_size = 1000;
    char * buf = (char *) malloc(sizeof (char) * buf_size);
    _count_down_t cd = {9, 0};

    tn_json_string_iterator_t *it = tn_json_string_iterator_new(&cd, _next_count_down, _free_nothing);
    uint64_t pos = 0l;

    uint64_t n;
    while ((n = tn_json_string_array_callback(it, pos, buf, buf_size)) != MHD_CONTENT_READER_END_OF_STREAM) {
        pos += n;
    }
    *(buf + pos) = '\0';
    tn_json_string_iterator_free(it);

    ck_assert_str_eq(buf, "[\"9\",\"8\",\"7\",\"6\",\"5\",\"4\",\"3\",\"2\",\"1\",\"0\"]");

    free(buf);
}

END_TEST


Suite * json_suite(void) {
    Suite *s;
    TCase *tc_empty, *tc_non_empty, *tc_free;

    s = suite_create("JSON");

    /* Core test case */
    tc_empty = tcase_create("Empty Array");
    tcase_add_test(tc_empty, test_empty_array);
    suite_add_tcase(s, tc_empty);

    tc_non_empty = tcase_create("Not Empty Array");
    tcase_add_test(tc_non_empty, test_non_empty_array);
    suite_add_tcase(s, tc_non_empty);
    
    tc_free = tcase_create("Free Tterator");
    tcase_add_test(tc_free, test_free);
    suite_add_tcase(s, tc_free);

    return s;

}

int main(void) {
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = json_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_MINIMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

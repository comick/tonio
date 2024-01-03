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

#include <check.h>
#include <microhttpd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "../src/json.h"

typedef struct _tokens_it {
    int current;
    const int count;
    const cj_token_t *tks;
} _tokens_it_t;

static cj_token_t _next_token(void *cls) {
    _tokens_it_t *cd = (_tokens_it_t *) cls;

    if (cd->current == cd->count) {
        return cj_eos;
    }

    int pos = cd->current;
    cd->current += 1;
    return cd->tks[pos];
}

static void _free_tokens_it(void *cls) {
    _tokens_it_t *cd = (_tokens_it_t *) cls;
    cd->current = -1;
}

static void cj_assert_tks_eq(cj_token_t tks[], int count, const char *exp) {
    size_t buf_size = 1000;
    char *buf = (char *) malloc(sizeof (char) * buf_size);

    _tokens_it_t tks_it = {0, count, tks};

    cj_token_stream_t *ts = cj_token_stream_new(&tks_it, _next_token, _free_tokens_it);
    uint64_t pos = 0l;

    uint64_t n;
    while ((n = cj_microhttpd_callback(ts, pos, buf + pos, buf_size)) != MHD_CONTENT_READER_END_OF_STREAM) {
        pos += n;
    }
    *(buf + pos) = '\0';

    cj_token_stream_free(ts);

    ck_assert_str_eq(buf, exp);
    ck_assert_int_eq(tks_it.current, -1);

    free(buf);
}

START_TEST(single_values) {
    cj_assert_tks_eq((cj_token_t[]){
        cj_null
    }, 1, "null");

    cj_assert_tks_eq((cj_token_t[]){
        cj_number(43.23)
    }, 1, "43.23");

    cj_assert_tks_eq((cj_token_t[]){
        cj_true
    }, 1, "true");

    cj_assert_tks_eq((cj_token_t[]){
        cj_false
    }, 1, "false");

    cj_assert_tks_eq((cj_token_t[]){
        cj_string("ciao")
    }, 1, "\"ciao\"");
}

END_TEST


START_TEST(empty_containers) {
    cj_assert_tks_eq((cj_token_t [2]){
        cj_object_push,
        cj_object_pop
    }, 2, "{}");

    cj_assert_tks_eq((cj_token_t [2]){
        cj_array_push,
        cj_array_pop
    }, 2, "[]");
}

END_TEST


START_TEST(test_non_empty_arrays) {
    cj_assert_tks_eq((cj_token_t [6]){
        cj_array_push,
        cj_string("ciao"),
        cj_null,
        cj_number(43),
        cj_string("bau"),
        cj_array_pop
    }, 6, "[\"ciao\",null,43,\"bau\"]");
}

END_TEST

START_TEST(test_non_empty_objects) {
    cj_assert_tks_eq((cj_token_t [8]){
        cj_object_push,
        cj_key("ciao"),
        cj_null,
        cj_key("miao"),
        cj_string("bau"),
        cj_key("bau"),
        cj_number(-5),
        cj_object_pop
    }, 8, "{\"ciao\":null,\"miao\":\"bau\",\"bau\":-5}");
}

END_TEST


Suite * cj_array_simple_suite(void) {
    Suite *s;
    TCase *tc_objs, *tc_empty_obj, *tc_non_empty, *tc_free;

    s = suite_create("Tokens serialization");

    /* JSON test cases */

    tc_empty_obj = tcase_create("Empty Containers");
    tcase_add_test(tc_empty_obj, empty_containers);
    suite_add_tcase(s, tc_empty_obj);

    tc_non_empty = tcase_create("Not Empty Arrays");
    tcase_add_test(tc_non_empty, test_non_empty_arrays);
    suite_add_tcase(s, tc_non_empty);

    tc_free = tcase_create("Single Values");
    tcase_add_test(tc_free, single_values);
    suite_add_tcase(s, tc_free);

    tc_objs = tcase_create("Non Empty Objects");
    tcase_add_test(tc_objs, test_non_empty_objects);
    suite_add_tcase(s, tc_objs);

    return s;

}

int main(void) {
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = cj_array_simple_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_VERBOSE);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

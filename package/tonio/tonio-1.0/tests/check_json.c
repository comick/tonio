/**
 * Copyright (c) 2022-2025 Michele Comignano <mcdev@playlinux.net>
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
#include <stdio.h>
#include <stdlib.h>

#include "../src/json.h"


static void cj_assert_tks_eq(int count, cj_token_t tks[count], const char *exp) {
    size_t buf_size = 1000;
    char buf[buf_size];

    cj_static_tokens_it_t tks_it = cj_static_tokens_it_of(count, tks);

    cj_token_stream_t *ts = cj_token_stream_new(&tks_it, cj_static_token_next, NULL);
    uint64_t pos = 0l;

    uint64_t n;
    while ((n = cj_token_stream_writer(ts, buf_size, buf + pos)) != CJ_END_OF_STREAM) {
        pos += n;
    }
    buf[pos] = '\0';

    cj_token_stream_free(ts);

    ck_assert_str_eq(buf, exp);
    ck_assert_int_eq(tks_it.current, tks_it.count);
}

START_TEST(single_values) {
    cj_assert_tks_eq(1, (cj_token_t[]){
        cj_null
    }, "null");

    cj_assert_tks_eq(1, (cj_token_t[]){
        cj_number(43.23)
    }, "43.23");

    cj_assert_tks_eq(1, (cj_token_t[]){
        cj_number(43.0)
    }, "43");

    cj_assert_tks_eq(1, (cj_token_t[]){
        cj_number(43)
    }, "43");

    cj_assert_tks_eq(1, (cj_token_t[]){
        cj_true
    }, "true");

    cj_assert_tks_eq(1, (cj_token_t[]){
        cj_false
    }, "false");

    cj_assert_tks_eq(1, (cj_token_t[]){
        cj_string(4, "ciao")
    }, "\"ciao\"");
}

END_TEST


START_TEST(empty_containers) {
    cj_assert_tks_eq(2, (cj_token_t [2]){
        cj_object_push,
        cj_object_pop
    }, "{}");

    cj_assert_tks_eq(2, (cj_token_t [2]){
        cj_array_push,
        cj_array_pop
    }, "[]");
}

END_TEST


START_TEST(test_non_empty_arrays) {
    cj_assert_tks_eq(6, (cj_token_t [6]){
        cj_array_push,
            cj_string(4, "ciao"),
            cj_null,
            cj_number(43),
            cj_string(3, "bau"),
        cj_array_pop
    }, "[\"ciao\",null,43,\"bau\"]");
}

END_TEST

START_TEST(test_non_empty_objects) {
    cj_assert_tks_eq(8, (cj_token_t [8]){
        cj_object_push,
            cj_key(4, "ciao"), cj_null,
            cj_key(4, "miao"), cj_string(3, "bau"),
            cj_key(3, "bau"), cj_number(-5),
        cj_object_pop
    }, "{\"ciao\":null,\"miao\":\"bau\",\"bau\":-5}");
}

END_TEST


Suite * cj_array_simple_suite(void) {
    Suite *s;
    TCase *tc_objs, *tc_empty_obj, *tc_non_empty, *tc_free;

    s = suite_create("CJ serialization");

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
    Suite *s = cj_array_simple_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_VERBOSE);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

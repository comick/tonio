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
#include <stdbool.h>

#include "../src/json.h"

static cj_token_t _next_none(void *cls) {
    char *status = (char *) cls;
    if (*status == '-') {
        *status = 'o';
        return cj_array_push;
    }
    return cj_array_pop;
}

typedef struct _words {
    int current;
    const int count;
    char **words;
} _words_t;

static cj_token_t _next_word(void *cls) {
    _words_t *cd = (_words_t *) cls;
    
    if (cd->current < 0) {
        cd->current = 0;
        return cj_array_push;
    }
    
    if (cd->current >= cd->count) {
        return cj_array_pop;
    }

    int pos = cd->current;
    cd->current += 1;
    return cj_string(cd->words[pos]);
}

static void _free_nothing(void *cls) {
}

static void _free_something(void *cls) {
    char *some_cls = (char *) cls;
    *some_cls = 'x';
}

START_TEST(test_free) {
    size_t buf_size = 1000;
    char *buf = (char *) malloc(sizeof (char) * buf_size);
    char some_cls = '-';
    cj_token_stream_t *it = cj_token_stream_new(&some_cls, _next_none, _free_something);
    uint64_t pos = 0l;

    uint64_t n;
    while ((n = cj_microhttpd_callback(it, pos, buf + pos, buf_size)) != MHD_CONTENT_READER_END_OF_STREAM) {
        pos += n;
    }
    *(buf + pos) = '\0';
    cj_token_stream_free(it);

    ck_assert_str_eq(buf, "[]");
    ck_assert_int_eq(some_cls, 'x');

    free(buf);
}

END_TEST


START_TEST(test_empty_array) {
    size_t buf_size = 1000;
    char * buf = (char *) malloc(sizeof (char) * buf_size);
    char some_cls = '-';
    cj_token_stream_t *it = cj_token_stream_new(&some_cls, _next_none, _free_nothing);
    uint64_t pos = 0l;

    uint64_t n;
    while ((n = cj_microhttpd_callback(it, pos, buf + pos, buf_size)) != MHD_CONTENT_READER_END_OF_STREAM) {
        pos += n;
    }
    *(buf + pos) = '\0';
    cj_token_stream_free(it);

    ck_assert_str_eq(buf, "[]");

    free(buf);
}

END_TEST


START_TEST(test_non_empty_array) {
    size_t buf_size = 1000;
    char * buf = (char *) malloc(sizeof (char) * buf_size);
    char *wsz[3] = {
        "ciao",
        "miao",
        "bau"
    };
    _words_t ws = {-1, 3, wsz};

    cj_token_stream_t *it = cj_token_stream_new(&ws, _next_word, _free_nothing);
    uint64_t pos = 0l;

    uint64_t n;
    while ((n = cj_microhttpd_callback(it, pos, buf + pos, buf_size)) != MHD_CONTENT_READER_END_OF_STREAM) {
        pos += n;
    }
    *(buf + pos) = '\0';
    cj_token_stream_free(it);

    ck_assert_str_eq(buf, "[\"ciao\",\"miao\",\"bau\"]");

    free(buf);
}

END_TEST


Suite * json_suite(void) {
    Suite *s;
    TCase *tc_empty, *tc_non_empty, *tc_words, *tc_free;

    s = suite_create("JSON");

    /* JSON test cases */
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

    srunner_run_all(sr, CK_VERBOSE);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

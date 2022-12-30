#include <check.h>
#include <microhttpd.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/json.h"

static char *_next_none(void *cls) {
    return NULL;
}

static void _free_nothing(void *cls) {

}

START_TEST(test_empty_array) {
    char * buf = (char *) malloc(sizeof (char) * 10);
    tn_json_string_iterator_t *it = tn_json_string_iterator_new(NULL, _next_none, _free_nothing);
    uint64_t len = 0l;

    uint64_t n;
    while ((n = tn_json_string_array_callback(it, len, buf, 1000000)) != MHD_CONTENT_READER_END_OF_STREAM) {
        len += n;
    }

    printf("%s", buf);

}

END_TEST

Suite * json_suite(void) {
    Suite *s;
    TCase *tc_core;

    s = suite_create("JSON");

    /* Core test case */
    tc_core = tcase_create("Empty Array");

    tcase_add_test(tc_core, test_empty_array);
    suite_add_tcase(s, tc_core);

    return s;

}

int main(void) {
    return 0;
}

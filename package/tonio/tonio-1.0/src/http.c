/**
 * Copyright (c) 2020 Michele Comignano <comick@gmail.com>
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
#include <sys/stat.h>
#include <fcntl.h>

#include "http.h"
#include "tonio.h"

#define MIME_JSON "application/json"
#define MIME_HTML "text/html"

static char *_card_present_json_fmt = "{\"present\":true,\"id\":\"%02X%02X%02X%02X\"}";
static char *_card_missing_json = "{\"present\":false}";

static struct MHD_Daemon *_mhd_daemon;
static struct MHD_Response *_root_response;

static int _handle_current_card(void *cls, struct MHD_Connection *connection,
        const char *url,
        const char *method, const char *version,
        const char *upload_data,
        size_t *upload_data_size, void **con_cls) {

    struct MHD_Response *response;
    int ret;
    char *page;
    long page_len = 0;
    uint8_t *card_id = cls;
    uint8_t card_present = card_id[0] | card_id[1] | card_id[2] | card_id[3];

    enum MHD_ResponseMemoryMode mem_mode;
    if (card_present) {
        page_len = snprintf(NULL, 0, _card_present_json_fmt,
                card_id[0], card_id[1], card_id[2], card_id[3]);
        page = malloc(page_len + 1);
        snprintf(page, page_len + 1, _card_present_json_fmt,
                card_id[0], card_id[1], card_id[2], card_id[3]);

        mem_mode = MHD_RESPMEM_MUST_FREE;
    } else {
        page = _card_missing_json;
        page_len = strlen(page);
        mem_mode = MHD_RESPMEM_PERSISTENT;
    }

    response = MHD_create_response_from_buffer(page_len,
            (void*) page, mem_mode);
    P_CHECK(response, return -1);

    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, MIME_JSON);
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);

    return ret;
}

static int _handle_root(void *cls, struct MHD_Connection *connection,
        const char *url,
        const char *method, const char *version,
        const char *upload_data,
        size_t *upload_data_size, void **con_cls) {

    int ret;

    syslog(LOG_DEBUG, "Root requested");

    ret = MHD_queue_response(connection, MHD_HTTP_OK, _root_response);

    return ret;
}

static int _handle_log(void *cls, struct MHD_Connection *connection,
        const char *url,
        const char *method, const char *version,
        const char *upload_data,
        size_t *upload_data_size, void **con_cls) {

    struct MHD_Response *response;
    int ret;
    struct stat log_stat;

    syslog(LOG_DEBUG, "Log requested");

    int log_fd = open("/var/log/messages", O_RDONLY);
    I_CHECK(log_fd, return MHD_NO);
    I_CHECK(fstat(log_fd, &log_stat), return MHD_NO);

    response = MHD_create_response_from_fd(log_stat.st_size, log_fd);

    P_CHECK(response, return MHD_NO);
    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, MIME_HTML);
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);

    return ret;
}

void tn_http_init(uint8_t *selected_card_id) {
    struct stat index_stat;

    _mhd_daemon = MHD_start_daemon(MHD_USE_EPOLL_INTERNAL_THREAD | MHD_USE_DUAL_STACK,
            PORT, NULL, NULL,
            &tn_http_handle_request, selected_card_id,
            MHD_OPTION_END);
    P_CHECK(_mhd_daemon, goto http_init_cleanup);

    int index_fd = open("/usr/share/tonio/index.html", O_RDONLY);
    I_CHECK(index_fd, return MHD_NO);
    I_CHECK(fstat(index_fd, &index_stat), return MHD_NO);

    _root_response = MHD_create_response_from_fd(index_stat.st_size, index_fd);
    MHD_add_response_header(_root_response, MHD_HTTP_HEADER_CONTENT_TYPE, MIME_HTML);


    syslog(LOG_INFO, "HTTP Initialized on port %d", PORT);
    return;

http_init_cleanup:

    syslog(LOG_ERR, "HTTP Initialization failed");

}

void tn_http_stop() {
    MHD_destroy_response(_root_response);
    MHD_stop_daemon(_mhd_daemon);
}

int tn_http_handle_request(void *cls, struct MHD_Connection *conn,
        const char *url,
        const char *method, const char *version,
        const char *upload_data,
        size_t *upload_data_size, void **con_cls) {

    int ret;

    if (strcmp("/status", url) == 0) {
        ret = _handle_current_card(cls, conn, url, method, version,
                upload_data, upload_data_size, con_cls);
    } else if (strcmp("/", url) == 0) {
        ret = _handle_root(cls, conn, url, method, version,
                upload_data, upload_data_size, con_cls);
    } else if (strcmp("/log", url) == 0) {
        ret = _handle_log(cls, conn, url, method, version,
                upload_data, upload_data_size, con_cls);
    }
    return ret;
}

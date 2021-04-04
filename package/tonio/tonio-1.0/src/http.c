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
#include <dirent.h>
#include <ctype.h>
#include <sys/syslog.h>
#include <iwlib.h>
#include <microhttpd.h>
#include <confuse.h>

#include "http.h"
#include "tonio.h"
#include "media.h"

#define INDEX_HTML_PATH "/usr/share/tonio/www/index.html"

#define MIME_JSON "application/json"
#define MIME_HTML "text/html"
#define MIME_TEXT_PLAIN "text/plain"

#define STATUS_TAG_PLAYING_JSON_FMT "{\"present\":true,\"id\":\"%02X%02X%02X%02X\",\"track_current\":%d,\"track_total\":%d,\"track_name\":\"%s\",\"internet\":false}"
#define STATUS_TAG_PRESENT_JSON_FMT "{\"present\":true,\"id\":\"%02X%02X%02X%02X\",\"internet\":false}"
#define STATUS_TAG_MISSING_JSON "{\"present\":false,\"internet\":false}"

#define SETTINGS_JSON_FMT "{\"essid\":\"%s\",\"pin_prev\":%d,\"pin_next\":%d,\"pin_volup\":%d,\"pin_voldown\":%d,\"pin_rfid\":%d,\"spi_rfid\":\"%s\"}"

#define LIBRARY_URL_PATH "/library"

struct tn_http {
    char *library_root;
    size_t library_root_len;

    struct MHD_Daemon *mhd_daemon;
    struct MHD_Response *root_response;
    tn_media_t *media;
    cfg_t *cfg;
    uint8_t *selected_card_id;
};

static int _handle_status(void *cls, struct MHD_Connection *connection,
        const char *url,
        const char *method, const char *version,
        const char *upload_data,
        size_t *upload_data_size, void **con_cls) {

    tn_http_t *self = (tn_http_t *) cls;
    struct MHD_Response *response;
    int ret;
    char *page = "";
    long page_len = 0;
    uint8_t *card_id = self->selected_card_id;
    uint8_t card_present = card_id[0] | card_id[1] | card_id[2] | card_id[3];

    enum MHD_ResponseMemoryMode mem_mode;
    if (tn_media_is_playing(self->media)) {
        int current_track = tn_media_track_current(self->media);
        int track_total = tn_media_track_total(self->media);
        char *track_name = tn_media_track_name(self->media);
        page_len = snprintf(NULL, 0, STATUS_TAG_PLAYING_JSON_FMT,
                card_id[0], card_id[1], card_id[2], card_id[3],
                current_track, track_total, track_name);
        page = malloc(page_len + 1);
        snprintf(page, page_len + 1, STATUS_TAG_PLAYING_JSON_FMT,
                card_id[0], card_id[1], card_id[2], card_id[3],
                current_track, track_total, track_name);

        mem_mode = MHD_RESPMEM_MUST_FREE;
    } else if (card_present) {
        page_len = snprintf(NULL, 0, STATUS_TAG_PRESENT_JSON_FMT,
                card_id[0], card_id[1], card_id[2], card_id[3]);
        page = malloc(page_len + 1);
        snprintf(page, page_len + 1, STATUS_TAG_PRESENT_JSON_FMT,
                card_id[0], card_id[1], card_id[2], card_id[3]);

        mem_mode = MHD_RESPMEM_MUST_FREE;
    } else {
        page = STATUS_TAG_MISSING_JSON;
        page_len = strlen(page);
        mem_mode = MHD_RESPMEM_PERSISTENT;
    }

    response = MHD_create_response_from_buffer(page_len,
            (void*) page, mem_mode);
    P_CHECK(response, return MHD_NO);

    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, MIME_JSON);
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);

    return ret;
}

static int _handle_settings(void *cls, struct MHD_Connection *connection,
        const char *url,
        const char *method, const char *version,
        const char *upload_data,
        size_t *upload_data_size, void **con_cls) {

    struct MHD_Response *response;
    int ret;
    char *page = "";
    long page_len = 0;
    tn_http_t *self = (tn_http_t *) cls;

    int iw_sock = iw_sockets_open();
    I_CHECK(iw_sock, return MHD_NO);
    wireless_config wconfig;
    I_CHECK(iw_get_basic_config(iw_sock, "wlan0", &wconfig), return MHD_NO);
    iw_sockets_close(iw_sock);

    int pin_prev = cfg_getint(self->cfg, CFG_BTN_TRACK_PREVIOUS);
    int pin_next = cfg_getint(self->cfg, CFG_BTN_TRACK_NEXT);
    int pin_vol_up = cfg_getint(self->cfg, CFG_BTN_VOLUME_UP);
    int pin_vol_down = cfg_getint(self->cfg, CFG_BTN_VOLUME_DOWN);
    int pin_rfid = cfg_getint(self->cfg, CFG_MFRC522_SWITCH);
    char *spi_dev = cfg_getstr(self->cfg, CFG_MFRC522_SPI_DEV);

    page_len = snprintf(NULL, 0, SETTINGS_JSON_FMT, wconfig.essid, pin_prev, pin_next, pin_vol_up, pin_vol_down, pin_rfid, spi_dev);
    page = malloc(page_len + 1);
    snprintf(page, page_len + 1, SETTINGS_JSON_FMT, wconfig.essid, pin_prev, pin_next, pin_vol_up, pin_vol_down, pin_rfid, spi_dev);

    response = MHD_create_response_from_buffer(page_len,
            (void*) page, MHD_RESPMEM_MUST_FREE);
    P_CHECK(response, return MHD_NO);

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

    tn_http_t *self = (tn_http_t *) cls;

    syslog(LOG_DEBUG, "Root requested");

    return MHD_queue_response(connection, MHD_HTTP_OK, self->root_response);
}

static enum MHD_Result _handle_log_offset_arg(void *cls,
        enum MHD_ValueKind kind,
        const char *key,
        const char *value) {
    if (kind == MHD_GET_ARGUMENT_KIND && strcmp("offset", key) == 0) {
        off_t *offset_arg = (off_t *) cls;
        size_t len = strlen(value);
        size_t i;
        for (i = 0; i < len && isdigit(value[i]); i++);
        if (i == len) *offset_arg = atol(value);

        return MHD_YES;
    } else {
        return MHD_NO;
    }
}

static int _handle_log(void *cls, struct MHD_Connection *connection,
        const char *url,
        const char *method, const char *version,
        const char *upload_data,
        size_t *upload_data_size, void **con_cls) {

    off_t log_offset = 0;
    struct MHD_Response *response;
    int ret;

    syslog(LOG_DEBUG, "Log requested");

    // For this to really work, syslogd should have rotation disable.
    int log_fd = open("/var/log/messages", O_RDONLY);
    off_t sz = lseek(log_fd, 0, SEEK_END);
    I_CHECK(sz, return MHD_NO);

    if (MHD_get_connection_values(connection, MHD_GET_ARGUMENT_KIND, _handle_log_offset_arg, &log_offset) > 0) {
        if (log_offset >= sz) {
            syslog(LOG_DEBUG, "Offset longer than the log itself: %ld vs %ld", log_offset, sz);
            log_offset = 0;
            // TODO this is whensyslog is rotated/flushed. send some header so html restart log thing.
        }
    }

    response = MHD_create_response_from_fd_at_offset64(sz - log_offset, log_fd, log_offset);

    P_CHECK(response, return MHD_NO);
    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, MIME_TEXT_PLAIN);
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);

    return ret;
}

typedef struct {
    int iwsocket;
    wireless_scan *scan_result;
    bool done;
} _iwlist_json_status_t;

static ssize_t _iwlist_json(void *cls, uint64_t pos, char *buf, size_t max) {
    _iwlist_json_status_t *status = cls;
    wireless_scan *scan_result = status->scan_result;
    const char *begin = "[";
    const char *end = "]";
    const char *quote = "\"";
    const char *sep = ",";

    if (status->done) return MHD_CONTENT_READER_END_OF_STREAM;

    if (pos == 0) {
        memcpy(buf, begin, strlen(begin));
        return strlen(begin);
    }

    if (scan_result == NULL) {
        status->done = true;
        memcpy(buf, end, strlen(end));
        return strlen(end);
    }

    int offset = 0;

    if (pos != strlen(begin)) {
        memcpy(buf + offset, sep, strlen(sep));
        offset += strlen(sep);
    }

    memcpy(buf + offset, quote, strlen(quote));
    offset += strlen(quote);

    char *essid = scan_result->b.essid;
    memcpy(buf + offset, essid, strlen(essid));
    offset += strlen(essid);
    status->scan_result = scan_result->next;
    free(scan_result);

    memcpy(buf + offset, quote, strlen(quote));
    offset += strlen(quote);

    return offset;
}

static void _iwlist_json_free(void *cls) {
    _iwlist_json_status_t *sts = cls;
    iw_sockets_close(sts->iwsocket);
    free(sts);
}

static int _handle_iwlist(void *cls, struct MHD_Connection *connection,
        const char *url,
        const char *method, const char *version,
        const char *upload_data,
        size_t *upload_data_size, void **con_cls) {

    struct MHD_Response *response;
    int ret;

    wireless_scan_head head;
    iwrange range;
    int sock;

    syslog(LOG_DEBUG, "Wireless list requested");

    sock = iw_sockets_open();
    I_CHECK(sock, return MHD_NO);
    I_CHECK(iw_get_range_info(sock, "wlan0", &range), return MHD_NO);
    I_CHECK(iw_scan(sock, "wlan0", range.we_version_compiled, &head), return MHD_NO);

    _iwlist_json_status_t *sts = malloc(sizeof (_iwlist_json_status_t));
    sts->scan_result = head.result;
    sts->done = false;
    sts->iwsocket = sock;

    response = MHD_create_response_from_callback(MHD_SIZE_UNKNOWN, 11, _iwlist_json, sts, _iwlist_json_free);
    P_CHECK(response, return MHD_NO);

    P_CHECK(response, return MHD_NO);
    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, MIME_TEXT_PLAIN);
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);

    return ret;
}

typedef struct {
    DIR *dir;
    bool done;
} _library_tags_json_status_t;

static ssize_t _library_tags_json(void *cls,
        uint64_t pos,
        char *buf,
        size_t max) {
    _library_tags_json_status_t *status = cls;
    DIR *dir = status->dir;
    const char *begin = "[";
    const char *end = "]";
    const char *quote = "\"";
    const char *sep = ",";
    struct dirent *ent;

    if (status->done) return MHD_CONTENT_READER_END_OF_STREAM;

    if (pos == 0) {
        memcpy(buf, begin, strlen(begin));
        return strlen(begin);
    }

    errno = 0;
    ent = readdir(dir);
    if (ent == NULL) {
        status->done = true;
        memcpy(buf, end, strlen(end));
        return strlen(end);
    }

    if (ent->d_type != DT_DIR || strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
        return 0;
    }

    int offset = 0;

    if (pos != strlen(begin)) {
        memcpy(buf + offset, sep, strlen(sep));
        offset += strlen(sep);
    }

    memcpy(buf + offset, quote, strlen(quote));
    offset += strlen(quote);

    memcpy(buf + offset, ent->d_name, strlen(ent->d_name));
    offset += strlen(ent->d_name);

    memcpy(buf + offset, quote, strlen(quote));
    offset += strlen(quote);

    return offset;
}

static void _library_tags_json_free(void *cls) {
    _library_tags_json_status_t *sts = cls;
    DIR *dir = sts->dir;
    closedir(dir);
    free(sts);
}

static int _handle_library(void *cls, struct MHD_Connection *connection,
        const char *url,
        const char *method, const char *version,
        const char *upload_data,
        size_t *upload_data_size, void **con_cls) {

    tn_http_t *self = (tn_http_t *) cls;
    struct MHD_Response *response;
    int ret;

    syslog(LOG_DEBUG, "Library requested");

    DIR *dir = opendir(self->library_root);
    P_CHECK(dir, return MHD_NO);

    _library_tags_json_status_t *sts = malloc(sizeof (_library_tags_json_status_t));
    sts->dir = dir;
    sts->done = false;

    response = MHD_create_response_from_callback(MHD_SIZE_UNKNOWN, 11, _library_tags_json, sts, _library_tags_json_free);
    P_CHECK(response, return MHD_NO);
    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, MIME_JSON);
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);

    return ret;
}

static int _handle_playlist(void *cls, struct MHD_Connection *connection,
        const char *url,
        const char *method, const char *version,
        const char *upload_data,
        size_t *upload_data_size, void **con_cls) {

    tn_http_t *self = (tn_http_t *) cls;
    struct MHD_Response *response;
    int ret;
    struct stat playlist_stat;

    const char *playlist_id = url + strlen(LIBRARY_URL_PATH) + 1;

    unsigned long int playlist_tag_long = strtoul(playlist_id, NULL, 16);
    uint8_t playlist_tag[4];
    playlist_tag[0] = playlist_tag_long >> 24 & 0xFF;
    playlist_tag[1] = playlist_tag_long >> 16 & 0xFF;
    playlist_tag[2] = playlist_tag_long >> 8 & 0xFF;
    playlist_tag[3] = playlist_tag_long >> 0 & 0xFF;

    char *file_name = find_playlist_filename(self->library_root, playlist_tag);

    syslog(LOG_DEBUG, "Playlist requested for %s: %s", playlist_id, file_name);

    int playlist_fd = open(file_name, O_RDONLY);
    I_CHECK(playlist_fd, return MHD_NO);
    I_CHECK(fstat(playlist_fd, &playlist_stat), return MHD_NO);

    response = MHD_create_response_from_fd(playlist_stat.st_size, playlist_fd);

    P_CHECK(response, return MHD_NO);
    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, MIME_TEXT_PLAIN);
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    free(file_name);

    return ret;
}

enum MHD_Result tn_http_handle_request(void *cls, struct MHD_Connection *conn,
        const char *url,
        const char *method, const char *version,
        const char *upload_data,
        size_t *upload_data_size, void **con_cls) {

    int ret = MHD_NO;

    if (strcmp("/status", url) == 0) {
        ret = _handle_status(cls, conn, url, method, version,
                upload_data, upload_data_size, con_cls);
    } else if (strcmp("/", url) == 0) {
        ret = _handle_root(cls, conn, url, method, version,
                upload_data, upload_data_size, con_cls);
    } else if (strcmp("/log", url) == 0) {
        ret = _handle_log(cls, conn, url, method, version,
                upload_data, upload_data_size, con_cls);
    } else if (strcmp("/iwlist", url) == 0) {
        ret = _handle_iwlist(cls, conn, url, method, version,
                upload_data, upload_data_size, con_cls);
    } else if (strcmp("/settings", url) == 0) {
        ret = _handle_settings(cls, conn, url, method, version,
                upload_data, upload_data_size, con_cls);
    } else if (strcmp(LIBRARY_URL_PATH, url) == 0) {
        ret = _handle_library(cls, conn, url, method, version,
                upload_data, upload_data_size, con_cls);
    } else if (strncmp(LIBRARY_URL_PATH, url, strlen(LIBRARY_URL_PATH)) == 0) {
        ret = _handle_playlist(cls, conn, url, method, version,
                upload_data, upload_data_size, con_cls);
    }

    return ret;
}

tn_http_t *tn_http_init(tn_media_t *media, uint8_t *selected_card_id, cfg_t *cfg) {
    struct stat index_stat;
    tn_http_t *self = malloc(sizeof (tn_http_t));
    P_CHECK(self, goto http_init_cleanup);
    self->media = media;
    self->selected_card_id = selected_card_id;
    self->library_root = cfg_getstr(cfg, CFG_MEDIA_ROOT);
    self->library_root_len = strlen(self->library_root);
    self->root_response = NULL;
    self->cfg = cfg;

    self->mhd_daemon = MHD_start_daemon(MHD_USE_EPOLL_INTERNAL_THREAD | MHD_USE_DUAL_STACK,
            PORT, NULL, NULL,
            &tn_http_handle_request, self,
            MHD_OPTION_END);
    P_CHECK(self->mhd_daemon, goto http_init_cleanup);

    int index_fd = open(INDEX_HTML_PATH, O_RDONLY);
    I_CHECK(index_fd, goto http_init_cleanup);
    I_CHECK(fstat(index_fd, &index_stat), goto http_init_cleanup);

    self->root_response = MHD_create_response_from_fd(index_stat.st_size, index_fd);
    MHD_add_response_header(self->root_response, MHD_HTTP_HEADER_CONTENT_TYPE, MIME_HTML);


    syslog(LOG_INFO, "HTTP Initialized on port %d", PORT);
    return self;

http_init_cleanup:
    syslog(LOG_ERR, "HTTP Initialization failed");
    return NULL;
}

void tn_http_stop(tn_http_t *self) {
    MHD_destroy_response(self->root_response);
    MHD_stop_daemon(self->mhd_daemon);
    free(self);
}

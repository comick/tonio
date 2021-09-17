/**
 * Copyright (c) 2020, 2021 Michele Comignano <comick@gmail.com>
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

#define STATIC_RES_ROOT "/usr/share/tonio/www"

#define MIME_APPLICATION_JSON "application/json"
#define MIME_TEXT_PLAIN "text/plain"
#define MIME_APPLICATION_JAVASCRIPT "application/javascript"
#define MIME_TEXT_CSS "text/css"
#define MIME_TEXT_HTML "text/html"

#define FILE_EXT_JS ".js"
#define FILE_EXT_CSS ".css"
#define FILE_EXT_HTML ".html"

#define FILE_EXT_JS_LEN strlen(FILE_EXT_JS)
#define FILE_EXT_CSS_LEN strlen(FILE_EXT_CSS)
#define FILE_EXT_HTML_LEN strlen(FILE_EXT_HTML)

#define JSON_TRUE "true"
#define JSON_FALSE "false"

#define STATUS_TAG_JSON_FMT "{\"present\":%s,\"id\":\"%02X%02X%02X%02X\",\"track_current\":%d,\"track_total\":%d,\"track_name\":\"%s\",\"internet\":%s}"

#define SETTINGS_JSON_FMT "{\"essid\":\"%s\",\"pin_prev\":%d,\"pin_next\":%d,\"pin_volup\":%d,\"pin_voldown\":%d,\"pin_rfid\":%d,\"spi_rfid\":\"%s\",\"gpio_chip\":\"%s\"}"

#define LIBRARY_URL_PATH "/library"

#define CFG_SETINT(K) if (strcmp(key, K)) { \
    cfg_setint(cfg, K, atoi(data)); \
}

#define CFG_SETSTR(K) if (strcmp(key, K)) { \
    cfg_setstr(cfg, K, data); \
}

struct tn_http {
    char *library_root;
    size_t library_root_len;

    struct MHD_Daemon *mhd_daemon;
    bool internet_connected;
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

    char *internet_status = self->internet_connected ? JSON_TRUE : JSON_FALSE;
    char *tag_present = (card_id[0] | card_id[1] | card_id[2] | card_id[3]) ? JSON_TRUE : JSON_FALSE;
    bool playing = tn_media_is_playing(self->media);
    int current_track, track_total = -1;
    char *track_name = "";

    if (playing) {
        current_track = tn_media_track_current(self->media);
        track_total = tn_media_track_total(self->media);
        track_name = tn_media_track_name(self->media);
    }

    page_len = snprintf(NULL, 0, STATUS_TAG_JSON_FMT, tag_present,
            card_id[0], card_id[1], card_id[2], card_id[3],
            current_track, track_total, track_name, internet_status);
    page = malloc(page_len + 1);
    snprintf(page, page_len + 1, STATUS_TAG_JSON_FMT, tag_present,
            card_id[0], card_id[1], card_id[2], card_id[3],
            current_track, track_total, track_name, internet_status);

    response = MHD_create_response_from_buffer(page_len,
            (void*) page, MHD_RESPMEM_MUST_FREE);
    P_CHECK(response, return MHD_NO);

    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, MIME_APPLICATION_JSON);
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);

    return ret;
}

static enum MHD_Result _process_settings(void *cls,
        enum MHD_ValueKind kind,
        const char *key,
        const char *filename,
        const char *content_type,
        const char *transfer_encoding,
        const char *data,
        uint64_t off,
        size_t size) {
    cfg_t *cfg = (cfg_t *) cls;

    CFG_SETINT(CFG_BTN_TRACK_PREVIOUS);
    CFG_SETINT(CFG_BTN_TRACK_NEXT);
    CFG_SETINT(CFG_BTN_VOLUME_UP);
    CFG_SETINT(CFG_BTN_VOLUME_DOWN);
    CFG_SETINT(CFG_MFRC522_SWITCH);
    CFG_SETSTR(CFG_MFRC522_SPI_DEV);
    CFG_SETSTR(CFG_GPIOD_CHIP_NAME);
    
    return MHD_YES;
}

static int _handle_settings(void *cls, struct MHD_Connection *connection,
        const char *url,
        const char *method, const char *version,
        const char *upload_data,
        size_t *upload_data_size, void **con_cls) {

    struct MHD_Response *response;
    int ret;

    tn_http_t *self = (tn_http_t *) cls;

    if (strcmp(method, MHD_HTTP_METHOD_POST) == 0) {
        struct MHD_PostProcessor *pp = *con_cls;
        if (pp == NULL) {
            pp = MHD_create_post_processor(connection, 1024, _process_settings, self->cfg);
            P_CHECK(pp, return MHD_NO);
            return MHD_YES;
        } else if (*upload_data_size > 0) {
            return MHD_post_process(pp, upload_data, *upload_data_size);
        } else {
            MHD_destroy_post_processor(pp);

            FILE *cfg_fp = fopen(self->cfg->filename, "w");
            P_CHECK(cfg_fp, return MHD_NO);
            I_CHECK(cfg_print(self->cfg, cfg_fp), return MHD_NO);
            I_CHECK(fclose(cfg_fp), return MHD_NO);
        }
    }

    char *page = "";
    long page_len = 0;

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
    char *gpio_chip = cfg_getstr(self->cfg, CFG_GPIOD_CHIP_NAME);

    page_len = snprintf(NULL, 0, SETTINGS_JSON_FMT, wconfig.essid, pin_prev, pin_next, pin_vol_up, pin_vol_down, pin_rfid, spi_dev, gpio_chip);
    page = malloc(page_len + 1);
    snprintf(page, page_len + 1, SETTINGS_JSON_FMT, wconfig.essid, pin_prev, pin_next, pin_vol_up, pin_vol_down, pin_rfid, spi_dev, gpio_chip);

    response = MHD_create_response_from_buffer(page_len, (void*) page, MHD_RESPMEM_MUST_FREE);
    P_CHECK(response, return MHD_NO);

    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, MIME_APPLICATION_JSON);

    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);

    return ret;
}

static int _handle_static(void *cls, struct MHD_Connection *connection,
        const char *url,
        const char *method, const char *version,
        const char *upload_data,
        size_t *upload_data_size, void **con_cls) {

    tn_http_t *self = (tn_http_t *) cls;
    struct MHD_Response *response = NULL;
    struct stat tmp_stat;
    unsigned int status_code = MHD_HTTP_OK;
    int tmp_fd = -1;
    char *static_filename = NULL;
    char *mime_str = MIME_TEXT_PLAIN;
    size_t static_filename_sz = 0;

    syslog(LOG_DEBUG, "Static or other requested: %s", url);

    if (strcmp("/", url) == 0) {
        url = "/index.html";
    }

    size_t url_len = strlen(url);
    
    static_filename_sz = strlen(STATIC_RES_ROOT) + url_len + 1;
    static_filename = malloc(static_filename_sz);
    snprintf(static_filename, static_filename_sz, "%s%s", STATIC_RES_ROOT, url);

    tmp_fd = open(static_filename, O_RDONLY);
    
    if (tmp_fd >= 0 && fstat(tmp_fd, &tmp_stat) >= 0) {
        // some simple ext matching to mimetype. libmagic, while a better solution is too big.
        if (url_len > FILE_EXT_HTML_LEN && strncmp(url + url_len - FILE_EXT_HTML_LEN, FILE_EXT_HTML, FILE_EXT_HTML_LEN) == 0) {
            mime_str = MIME_TEXT_HTML;
        } else if (url_len > FILE_EXT_CSS_LEN && strncmp(url + url_len - FILE_EXT_CSS_LEN, FILE_EXT_CSS, FILE_EXT_CSS_LEN) == 0) {
            mime_str = MIME_TEXT_CSS;
        } else if (url_len > FILE_EXT_JS_LEN && strncmp(url + url_len - FILE_EXT_JS_LEN, FILE_EXT_JS, FILE_EXT_JS_LEN) == 0) {
            mime_str = MIME_APPLICATION_JAVASCRIPT;
        }
        response = MHD_create_response_from_fd(tmp_stat.st_size, tmp_fd);
    } else if (tmp_fd < 0 && errno == EACCES) {
        syslog(LOG_INFO, "Resource requested but not found at %s (%s)", url, static_filename);
        status_code = MHD_HTTP_NOT_FOUND;
        response = MHD_create_response_from_buffer(strlen("Not Found"), "Not Found", MHD_RESPMEM_MUST_COPY);
    } else {
        syslog(LOG_ERR, "Could not serve %s (static resource %s): %s %d", url, static_filename, strerror(errno), tmp_fd);
        status_code = MHD_HTTP_INTERNAL_SERVER_ERROR;
        response = MHD_create_response_from_buffer(strlen("Internal Server Error"), "Internal Server Error", MHD_RESPMEM_MUST_COPY);
    }

    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, mime_str);
    return MHD_queue_response(connection, status_code, response);
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
    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, MIME_APPLICATION_JSON);
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
    } else {
        ret = _handle_static(cls, conn, url, method, version,
                upload_data, upload_data_size, con_cls);
    }

    return ret;
}

tn_http_t *tn_http_init(tn_media_t *media, uint8_t *selected_card_id, cfg_t *cfg) {
    tn_http_t *self = malloc(sizeof (tn_http_t));
    P_CHECK(self, goto http_init_cleanup);
    self->media = media;
    self->selected_card_id = selected_card_id;
    self->library_root = cfg_getstr(cfg, CFG_MEDIA_ROOT);
    self->library_root_len = strlen(self->library_root);
    self->cfg = cfg;

    self->internet_connected = false;

    self->mhd_daemon = MHD_start_daemon(MHD_USE_EPOLL_INTERNAL_THREAD,
            PORT, NULL, NULL,
            &tn_http_handle_request, self,
            MHD_OPTION_END);
    P_CHECK(self->mhd_daemon, goto http_init_cleanup);

    syslog(LOG_INFO, "HTTP Initialized on port %d", PORT);
    return self;

http_init_cleanup:
    syslog(LOG_ERR, "HTTP Initialization failed");
    return NULL;
}

void tn_http_stop(tn_http_t *self) {
    MHD_stop_daemon(self->mhd_daemon);
    free(self);
}

/**
 * Copyright (c) 2020-2024 Michele Comignano <mcdev@playlinux.net>
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
#include <signal.h>

#include "http.h"
#include "tonio.h"
#include "media.h"
#include "json.h"

// TODO make configurable
//#define WLAN_IF "wlp9s0"
#define WLAN_IF "wlan0"
//#define SYSLOG_PATH "/var/log/syslog"
#define SYSLOG_PATH "/var/log/messages"

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

#define SETTINGS_JSON_FMT "{\"essid\":\"%s\",\"pin_prev\":%lu,\"pin_next\":%lu,\"pin_volup\":%lu,\"pin_voldown\":%lu,\"pin_rfid\":%lu,\"spi_rfid\":\"%s\",\"gpio_chip\":\"%s\",\"factory_new\":%s}"

#define LIBRARY_URL_PATH "/library"

#define CFG_SETBOOL(K) if (strcmp(key, K) == 0) { \
    cfg_setbool(cfg, K, strcmp(cj_true.value.str, d) == 0 ? cfg_true : cfg_false); \
}

#define CFG_SETINT(K) if (strcmp(key, K) == 0) { \
    cfg_setint(cfg, K, strtol(d, NULL, 10)); \
}

#define CFG_SETSTR(K) if (strcmp(key, K) == 0) { \
    cfg_setstr(cfg, K, d); \
}

struct tn_http {
    char *library_root;

    int http_port;
    char *http_root;
    cj_token_t status_tks[12];
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
    uint32_t card_id =
            self->selected_card_id[0] << 24 |
            self->selected_card_id[1] << 16 |
            self->selected_card_id[2] << 8 |
            self->selected_card_id[3];

    bool playing = tn_media_is_playing(self->media);

    self->status_tks[2] = card_id ? cj_number(card_id) : cj_null;
    self->status_tks[4] = playing ? cj_number(tn_media_track_current(self->media)) : cj_null;
    self->status_tks[6] = playing ? cj_number(tn_media_track_total(self->media)) : cj_null;
    self->status_tks[8] = playing ? cj_string(tn_media_track_name(self->media)) : cj_null;
    self->status_tks[10] = self->internet_connected ? cj_true : cj_false;

    cj_tokens_it_t *tks_it = malloc(sizeof (cj_tokens_it_t));
    tks_it->current = 0;
    tks_it->count = 12;
    tks_it->tks = self->status_tks;


    cj_token_stream_t *status_ts = cj_token_stream_new(tks_it, cj_next_token, NULL);

    response = MHD_create_response_from_callback(MHD_SIZE_UNKNOWN, 11,
            cj_microhttpd_callback,
            status_ts,
            cj_token_stream_free
            );

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

    char d[size + 1];
    strncpy(d, data + off, size);
    d[size] = 0;

    CFG_SETBOOL(CFG_FACTORY_NEW);
    CFG_SETINT(CFG_BTN_TRACK_PREVIOUS);
    CFG_SETINT(CFG_BTN_TRACK_NEXT);
    CFG_SETINT(CFG_BTN_VOLUME_UP);
    CFG_SETINT(CFG_BTN_VOLUME_DOWN);
    CFG_SETINT(CFG_MFRC522_SWITCH);
    CFG_SETSTR(CFG_MFRC522_SPI_DEV);
    CFG_SETSTR(CFG_GPIOD_CHIP_NAME);

    return MHD_YES;
}

static enum MHD_Result _handle_settings(void *cls, struct MHD_Connection *connection,
        const char *url,
        const char *method, const char *version,
        const char *upload_data,
        size_t *upload_data_size, void **con_cls) {

    struct MHD_Response *response;
    enum MHD_Result ret;

    tn_http_t *self = (tn_http_t *) cls;

    if (strcmp(method, MHD_HTTP_METHOD_POST) == 0) {
        struct MHD_PostProcessor *pp = *con_cls;
        if (pp == NULL) {
            pp = MHD_create_post_processor(connection, 1024, _process_settings, self->cfg);
            P_CHECK(pp, return MHD_NO);
            *con_cls = pp;
            return MHD_YES;
        }

        if (*upload_data_size > 0) {
            MHD_post_process(pp, upload_data, *upload_data_size);
            *upload_data_size = 0;
            return MHD_YES;
        } else {
            MHD_destroy_post_processor(pp);

            FILE *cfg_fp = fopen(self->cfg->filename, "w");
            P_CHECK(cfg_fp, return MHD_NO);
            I_CHECK(cfg_print(self->cfg, cfg_fp), return MHD_NO);
            I_CHECK(fclose(cfg_fp), return MHD_NO);
            raise(SIGUSR1);
        }
    }

    char *page = "";
    long page_len = 0;

    int iw_sock = iw_sockets_open();
    I_CHECK(iw_sock, return MHD_NO);
    wireless_config wconfig;
    I_CHECK(iw_get_basic_config(iw_sock, WLAN_IF, &wconfig), return MHD_NO);
    iw_sockets_close(iw_sock);

    const char *factory_new = cfg_getbool(self->cfg, CFG_FACTORY_NEW) == cfg_true
            ? cj_true.value.str
            : cj_false.value.str;
    long pin_prev = cfg_getint(self->cfg, CFG_BTN_TRACK_PREVIOUS);
    long pin_next = cfg_getint(self->cfg, CFG_BTN_TRACK_NEXT);
    long pin_vol_up = cfg_getint(self->cfg, CFG_BTN_VOLUME_UP);
    long pin_vol_down = cfg_getint(self->cfg, CFG_BTN_VOLUME_DOWN);
    long pin_rfid = cfg_getint(self->cfg, CFG_MFRC522_SWITCH);
    char *spi_dev = cfg_getstr(self->cfg, CFG_MFRC522_SPI_DEV);
    char *gpio_chip = cfg_getstr(self->cfg, CFG_GPIOD_CHIP_NAME);

    page_len = snprintf(NULL, 0, SETTINGS_JSON_FMT, wconfig.essid, pin_prev, pin_next, pin_vol_up, pin_vol_down, pin_rfid, spi_dev, gpio_chip, factory_new);
    page = malloc(page_len + 1);
    snprintf(page, page_len + 1, SETTINGS_JSON_FMT, wconfig.essid, pin_prev, pin_next, pin_vol_up, pin_vol_down, pin_rfid, spi_dev, gpio_chip, factory_new);

    response = MHD_create_response_from_buffer(page_len, (void*) page, MHD_RESPMEM_MUST_FREE);
    P_CHECK(response, return MHD_NO);

    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, MIME_APPLICATION_JSON);

    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);

    return ret;
}

static enum MHD_Result _handle_static(void *cls, struct MHD_Connection *connection,
        const char *url,
        const char *method, const char *version,
        const char *upload_data,
        size_t *upload_data_size, void **con_cls) {

    struct MHD_Response *response = NULL;
    struct stat tmp_stat;
    unsigned int status_code = MHD_HTTP_OK;
    int tmp_fd = -1;
    char *static_filename = NULL;
    char *mime_str = MIME_TEXT_PLAIN;
    size_t static_filename_sz = 0;
    tn_http_t *self = (tn_http_t *) cls;

    syslog(LOG_DEBUG, "Static or other requested: %s", url);

    if (strcmp("/", url) == 0) {
        url = "/index.html";
    }

    size_t url_len = strlen(url);

    static_filename_sz = strlen(self->http_root) + url_len + 1;
    static_filename = malloc(static_filename_sz);
    snprintf(static_filename, static_filename_sz, "%s%s", self->http_root, url);

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
        // TODO not working if folder is wrong, fixme.
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
        if (i == len) *offset_arg = strtol(value, NULL, 10);

        return MHD_YES;
    } else {
        return MHD_NO;
    }
}

static enum MHD_Result _handle_log(void *cls, struct MHD_Connection *connection,
        const char *url,
        const char *method, const char *version,
        const char *upload_data,
        size_t *upload_data_size, void **con_cls) {

    off_t log_offset = 0;
    struct MHD_Response *response;
    enum MHD_Result ret;

    syslog(LOG_DEBUG, "Log requested");

    // For this to really work, syslogd should have rotation disable.
    int log_fd = open(SYSLOG_PATH, O_RDONLY); // TODO add one more log file path
    off_t sz = lseek(log_fd, 0, SEEK_END);
    I_CHECK(sz, return MHD_NO);

    if (MHD_get_connection_values(connection, MHD_GET_ARGUMENT_KIND, _handle_log_offset_arg, &log_offset) > 0) {
        if (log_offset >= sz) {
            syslog(LOG_DEBUG, "Offset longer than the log itself: %lld vs %lld", (unsigned long long) log_offset, (unsigned long long) sz);
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

static cj_token_t _iwlist_json_next(void *cls) {
    _iwlist_json_status_t *sts = (_iwlist_json_status_t *) cls;

    if (sts->iwsocket == 0) {
        sts->done = false;
        wireless_scan_head head;
        iwrange range;

        sts->iwsocket = iw_sockets_open();
        I_CHECK(sts->iwsocket, return cj_null);
        I_CHECK(iw_get_range_info(sts->iwsocket, WLAN_IF, &range), return cj_null);
        I_CHECK(iw_scan(sts->iwsocket, WLAN_IF, range.we_version_compiled, &head), return cj_null);

        sts->scan_result = head.result;
        return cj_array_push;
    }

    if (sts->scan_result == NULL && !sts->done) {
        sts->done = true;
        return cj_array_pop;
    } else if (sts->scan_result == NULL && sts->done) {
        return cj_eos;
    } else {
        char *res = sts->scan_result->b.essid;
        sts->scan_result = sts->scan_result->next;
        return cj_string(res);
    }
}

static void _iwlist_json_free(void *cls) {
    _iwlist_json_status_t *sts = (_iwlist_json_status_t *) cls;
    if (sts->iwsocket != 0) {
        iw_sockets_close(sts->iwsocket);
    }
    free(sts);
}

static int _handle_iwlist(void *cls, struct MHD_Connection *connection,
        const char *url,
        const char *method, const char *version,
        const char *upload_data,
        size_t *upload_data_size, void **con_cls) {

    struct MHD_Response *response;
    int ret;

    syslog(LOG_DEBUG, "Wireless list requested");

    _iwlist_json_status_t *sts = calloc(1, sizeof (_iwlist_json_status_t));

    cj_token_stream_t *iwlist_it = cj_token_stream_new(sts, _iwlist_json_next, _iwlist_json_free);

    response = MHD_create_response_from_callback(MHD_SIZE_UNKNOWN, 35,
            cj_microhttpd_callback,
            iwlist_it,
            cj_token_stream_free
            );

    P_CHECK(response, return MHD_NO);
    MHD_add_response_header(response, MHD_HTTP_HEADER_CONTENT_TYPE, MIME_APPLICATION_JSON);
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);

    return ret;
}

struct _library_tags_cls {
    DIR *dir;
    char *library_root;
    bool done;
};

static cj_token_t _library_tags_json_next(void *cls) {
    struct _library_tags_cls *info = (struct _library_tags_cls *) cls;
    if (info ->dir == NULL) {
        info->done = false;
        info->dir = opendir(info->library_root);
        P_CHECK(info->dir, 3);
        return cj_array_push;
    }

    DIR *dir = info->dir;
    struct dirent *ent;

    errno = 0;
    ent = readdir(dir);
    while (ent != NULL && (ent->d_type != DT_DIR || strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)) {
        ent = readdir(dir);
    }
    if (ent != NULL) {
        return cj_string(ent->d_name);
    } else if (!info->done) {
        info->done = true;
        return cj_array_pop;
    } else {
        return cj_eos;
    }
}

static void _library_tags_json_free(void *cls) {
    struct _library_tags_cls *info = (struct _library_tags_cls *) cls;

    DIR *dir = info->dir;
    if (dir != NULL) closedir(dir);
    free(info);
}

static enum MHD_Result _handle_library(void *cls, struct MHD_Connection *connection,
        const char *url,
        const char *method, const char *version,
        const char *upload_data,
        size_t *upload_data_size, void **con_cls) {

    tn_http_t *self = (tn_http_t *) cls;
    struct MHD_Response *response;
    enum MHD_Result ret;

    syslog(LOG_DEBUG, "Library requested");
    syslog(LOG_ERR, "%s", self->library_root);

    struct _library_tags_cls *lib_cls = malloc(sizeof (struct _library_tags_cls));
    lib_cls->dir = NULL;
    lib_cls->library_root = self->library_root;
    cj_token_stream_t *dir_it = cj_token_stream_new(lib_cls, _library_tags_json_next, _library_tags_json_free);

    response = MHD_create_response_from_callback(MHD_SIZE_UNKNOWN, 11,
            cj_microhttpd_callback,
            dir_it,
            cj_token_stream_free
            );
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
    self->http_port = cfg_getint(cfg, CFG_HTTP_PORT);
    self->http_root = cfg_getstr(cfg, CFG_HTTP_ROOT);
    self->cfg = cfg;

    self->internet_connected = false;

    self->status_tks[0] = cj_object_push;
    self->status_tks[1] = cj_key("card_id");
    self->status_tks[2] = cj_null;
    self->status_tks[3] = cj_key("track_current");
    self->status_tks[4] = cj_null;
    self->status_tks[5] = cj_key("track_total");
    self->status_tks[6] = cj_null;
    self->status_tks[7] = cj_key("track_name");
    self->status_tks[8] = cj_null;
    self->status_tks[9] = cj_key("internet");
    self->status_tks[10] = self->internet_connected ? cj_true : cj_false;
    self->status_tks[11] = cj_object_pop;

    self->mhd_daemon = MHD_start_daemon(MHD_USE_EPOLL_INTERNAL_THREAD,
            self->http_port, NULL, NULL,
            &tn_http_handle_request, self,
            MHD_OPTION_END);
    P_CHECK(self->mhd_daemon, goto http_init_cleanup);

    syslog(LOG_INFO, "HTTP Initialized on port %d", self->http_port);
    return self;

http_init_cleanup:
    syslog(LOG_ERR, "HTTP Initialization failed");
    return NULL;
}

void tn_http_stop(tn_http_t *self) {
    syslog(LOG_INFO, "Shutting down http subsystem.");
    MHD_stop_daemon(self->mhd_daemon);
    free(self);
}

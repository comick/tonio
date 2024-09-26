/**
 * Copyright (c) 2020-2023 Michele Comignano <mcdev@playlinux.net>
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <string.h>
#include <dirent.h>
#include <gpiod.h>
#include <stdbool.h>
#include <confuse.h>
#include <signal.h>

#include "config.h"
#include "tonio.h"
#include "http.h"
#include "media.h"
#include "input.h"


static cfg_opt_t config_opts[] = {
    CFG_BOOL(CFG_FACTORY_NEW, cfg_true, CFGF_NONE),
    CFG_INT(CFG_HTTP_PORT, 80, CFGF_NONE),
    CFG_STR(CFG_HTTP_ROOT, "/usr/share/tonio/www", CFGF_NONE),
    CFG_STR(CFG_MEDIA_ROOT, "/mnt/media", CFGF_NONE),
    CFG_STR(CFG_MEDIA_AUDIO_OUT, "alsa", CFGF_NONE),
    CFG_STR(CFG_GPIOD_CHIP_NAME, "gpiochip0", CFGF_NONE),
    CFG_STR(CFG_MIXER_CARD, "default", CFGF_NONE),
    CFG_STR(CFG_MIXER_SELEM, "Headphone", CFGF_NONE),
    CFG_FLOAT(CFG_VOLUME_MAX, 0.7, CFGF_NONE),
    CFG_INT(CFG_BTN_TRACK_PREVIOUS, 18, CFGF_NONE),
    CFG_INT(CFG_BTN_TRACK_NEXT, 23, CFGF_NONE),
    CFG_INT(CFG_BTN_VOLUME_DOWN, 21, CFGF_NONE),
    CFG_INT(CFG_BTN_VOLUME_UP, 24, CFGF_NONE),
    CFG_INT(CFG_MFRC522_SWITCH, 25, CFGF_NONE),
    CFG_STR(CFG_MFRC522_SPI_DEV, "/dev/spidev0.0", CFGF_NONE),
    CFG_END()
};

static char **_argv = NULL;

static bool _reload_requested = false;

static struct timespec _poll_interval = {
    .tv_sec = 0,
    .tv_nsec = 50/* msec */ * 1000000
};

static cfg_t *cfg = NULL;
static tn_media_t *media = NULL;
static tn_input_t *input = NULL;
static tn_http_t *http = NULL;

void _request_reload() {
    syslog(LOG_ALERT, "Reload requested.");
    _reload_requested = true;
}

static void _shutdown() {
    syslog(LOG_ALERT, "Shutting down.");
    if (input != NULL) tn_input_destroy(input);
    if (media != NULL) tn_media_destroy(media);
    if (http != NULL) tn_http_stop(http);
    if (cfg != NULL) cfg_free(cfg);
    closelog();
}

static void _keep_alive() {
    if (_reload_requested) {
        syslog(LOG_ALERT, "Reload requested.");
        _shutdown();
        execv(_argv[0], _argv);
    }
    nanosleep(&_poll_interval, NULL);
}

int main(int argc, char** argv) {

    //Recognized card ID
    uint8_t card_id[5] = {0x00,};
    uint8_t selected_card_id[5] = {0x00,};

    openlog("tonio", LOG_PID | LOG_CONS | LOG_PERROR, LOG_USER);

    if (argc < 2) {
        syslog(LOG_CRIT, "Exactly one argument is expected, pointing to the configuration.");
        return EXIT_FAILURE;
    }

    cfg = cfg_init(config_opts, CFGF_NONE);

    if (!cfg || cfg_parse(cfg, argv[1]) == CFG_PARSE_ERROR) {
        syslog(LOG_CRIT, "Cannot parse configuration at %s.", argv[1]);
        return EXIT_FAILURE;
    }

    media = tn_media_init(cfg);
    P_CHECK(media, return EXIT_FAILURE);

    input = tn_input_init(cfg);

    http = tn_http_init(media, selected_card_id, cfg);
    P_CHECK(http, return EXIT_FAILURE);

    _argv = argv;

    signal(SIGUSR1, _request_reload);

    while (true) {

        syslog(LOG_INFO, "Waiting for card...");

        while (!tn_input_tag_poll(input, card_id)) {
            _keep_alive();
        }

        syslog(LOG_INFO, "Card uid detected: %02X%02X%02X%02X", card_id[0], card_id[1], card_id[2], card_id[3]);

        if (!tn_input_tag_select(input, card_id)) {
            syslog(LOG_ERR, "Card select failed");
        } else {

            memcpy(&selected_card_id, &card_id, sizeof (card_id));

            if (tn_media_play(media, card_id)) {

                int last_prev_state = tn_input_btn_prev(input);
                int last_next_state = tn_input_btn_next(input);

                // Does some auth to make sure card still there.
                while (tn_input_tag_check(input, card_id)) {
                    _keep_alive();

                    int current_prev_state = tn_input_btn_prev(input);
                    if (current_prev_state == 1 && last_prev_state == 0) {
                        tn_media_previous(media);
                    }
                    last_prev_state = current_prev_state;

                    int current_next_state = tn_input_btn_next(input);
                    if (current_next_state == 1 && last_next_state == 0) {
                        tn_media_next(media);
                    }
                    last_next_state = current_next_state;

                    // volume kept continuous, more pleasant. TODO maybe add some timer to make is slower
                    if (tn_input_btn_vol_down(input) > 0) {
                        tn_media_volume_down(media);
                    }
                    if (tn_input_btn_vol_up(input) > 0) {
                        tn_media_volume_up(media);
                    }

                }

            } else {
                // Does some auth to make sure card still there.
                // Allows http to know the card is there
                while (tn_input_tag_check(input, card_id)) {
                    _keep_alive();
                }

            }

            memset(&selected_card_id, 0x00, sizeof (selected_card_id));
            tn_media_stop(media);

            syslog(LOG_INFO, "Card removed");

        }
    }

    return (EXIT_SUCCESS);
}

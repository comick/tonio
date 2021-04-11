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

#include <stdio.h>
#include <stdlib.h>
#include "mfrc522.h"
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <string.h>
#include <dirent.h>
#include <gpiod.h>
#include <stdbool.h>
#include <confuse.h>

#include "tonio.h"
#include "http.h"
#include "media.h"


static cfg_opt_t config_opts[] = {
    CFG_STR(CFG_MEDIA_ROOT, "/mnt/media", CFGF_NONE),
    CFG_STR(CFG_MIXER_CARD, "default", CFGF_NONE),
    CFG_STR(CFG_MIXER_SELEM, "PCM", CFGF_NONE),
    CFG_FLOAT(CFG_VOLUME_MAX, 0.7, CFGF_NONE),
    CFG_INT(CFG_BTN_TRACK_PREVIOUS, 18, CFGF_NONE),
    CFG_INT(CFG_BTN_TRACK_NEXT, 23, CFGF_NONE),
    CFG_INT(CFG_BTN_VOLUME_DOWN, 24, CFGF_NONE),
    CFG_INT(CFG_BTN_VOLUME_UP, 21, CFGF_NONE),
    CFG_INT(CFG_MFRC522_SWITCH, 25, CFGF_NONE),
    CFG_STR(CFG_MFRC522_SPI_DEV, "/dev/spidev0.0", CFGF_NONE),
    CFG_END()
};

int main(int argc, char** argv) {

    MFRC522_Status_t ret;
    uint8_t ret_int;
    //Recognized card ID
    uint8_t card_id[5] = {0x00,};
    uint8_t selected_card_id[5] = {0x00,};
    struct gpiod_chip *gpio_chip = NULL;
    struct gpiod_line *gpio_mfrc522_line, *vol_up_line, *vol_down_line, *track_next_line, *track_prev_line = NULL;

    uint8_t default_key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    struct timespec poll_interval = {
        .tv_sec = 0,
        .tv_nsec = 50/* msec */ * 1000000
    };

    openlog("tonio", LOG_PID | LOG_CONS | LOG_PERROR, LOG_USER);

    cfg_t *cfg = cfg_init(config_opts, CFGF_NONE);

    if (!cfg || cfg_parse(cfg, argv[1]) == CFG_PARSE_ERROR) {
        syslog(LOG_CRIT, "Cannor parse configuration at %s.", argv[1]);
        return EXIT_FAILURE;
    }

    tn_media_t *media = tn_media_init(cfg);

    // TODO make this configured
    P_CHECK(gpio_chip = gpiod_chip_open_by_name("gpiochip0"), return EXIT_FAILURE);
    P_CHECK(gpio_mfrc522_line = gpiod_chip_get_line(gpio_chip, cfg_getint(cfg, CFG_MFRC522_SWITCH)), return EXIT_FAILURE);

    ret = MFRC522_Init(gpio_mfrc522_line, cfg_getstr(cfg, CFG_MFRC522_SPI_DEV), 'B');

    if (ret < 0) {
        syslog(LOG_CRIT, "RFID Failed to initialize");
        exit(EXIT_FAILURE);
    }
    syslog(LOG_INFO, "RFID reader successfully initialized");

    P_CHECK(track_next_line = gpiod_chip_get_line(gpio_chip, cfg_getint(cfg, CFG_BTN_TRACK_NEXT)), return EXIT_FAILURE);
    P_CHECK(track_prev_line = gpiod_chip_get_line(gpio_chip, cfg_getint(cfg, CFG_BTN_TRACK_PREVIOUS)), return EXIT_FAILURE);
    P_CHECK(vol_up_line = gpiod_chip_get_line(gpio_chip, cfg_getint(cfg, CFG_BTN_VOLUME_UP)), return EXIT_FAILURE);
    P_CHECK(vol_down_line = gpiod_chip_get_line(gpio_chip, cfg_getint(cfg, CFG_BTN_VOLUME_DOWN)), return EXIT_FAILURE);

    I_CHECK(gpiod_line_request_input(track_next_line, "tonio"), return EXIT_FAILURE);
    I_CHECK(gpiod_line_request_input(track_prev_line, "tonio"), return EXIT_FAILURE);
    I_CHECK(gpiod_line_request_input(vol_up_line, "tonio"), return EXIT_FAILURE);
    I_CHECK(gpiod_line_request_input(vol_down_line, "tonio"), return EXIT_FAILURE);

    tn_http_t *http = tn_http_init(media, selected_card_id, cfg);

    while (true) {

        syslog(LOG_INFO, "Waiting for card...");

        while (MFRC522_Check(card_id) != MI_OK) {
            nanosleep(&poll_interval, NULL);
        }

        syslog(LOG_INFO, "Card uid detected: %02X%02X%02X%02X", card_id[0], card_id[1], card_id[2], card_id[3]);

        if (MFRC522_SelectTag(card_id) == 0) {
            syslog(LOG_ERR, "Card select failed");
        } else {

            syslog(LOG_INFO, "Card type: %s", MFRC522_TypeToString(MFRC522_ParseType(ret_int)));

            memcpy(&selected_card_id, &card_id, sizeof (card_id));

            if (tn_media_play(media, card_id)) {

                int last_prev_state = gpiod_line_get_value(track_prev_line);
                int last_next_state = gpiod_line_get_value(track_next_line);

                // Does some auth to make sure card still there.
                while (MFRC522_Auth((uint8_t) PICC_AUTHENT1A, (uint8_t) 0,
                        (uint8_t*) default_key, (uint8_t*) card_id) == MI_OK) {
                    nanosleep(&poll_interval, NULL);

                    int current_prev_state = gpiod_line_get_value(track_prev_line);
                    if (current_prev_state == 1 && last_prev_state == 0) {
                        tn_media_previous(media);
                    }
                    last_prev_state = current_prev_state;

                    int current_next_state = gpiod_line_get_value(track_next_line);
                    if (current_next_state == 1 && last_next_state == 0) {
                        tn_media_next(media);
                    }
                    last_next_state = current_next_state;

                    // volume kept continuous, more pleasant. TODO maybe add some timer to make is slower
                    if (gpiod_line_get_value(vol_up_line) == 1) tn_media_volume_down(media);
                    if (gpiod_line_get_value(vol_down_line) == 1) tn_media_volume_up(media);

                }

            } else {
                // Does some auth to make sure card still there.
                // Allows http to know the card is there
                while (MFRC522_Auth((uint8_t) PICC_AUTHENT1A, (uint8_t) 0,
                        (uint8_t*) default_key, (uint8_t*) card_id) == MI_OK) {
                    nanosleep(&poll_interval, NULL);
                }

            }

            memset(&selected_card_id, 0x00, sizeof (selected_card_id));
            tn_media_stop(media);

            syslog(LOG_INFO, "Card removed");

        }
    }

    MFRC522_Halt();

    tn_media_destroy(media);
    tn_http_stop(http);

    cfg_free(cfg);

    closelog();

    return (EXIT_SUCCESS);
}

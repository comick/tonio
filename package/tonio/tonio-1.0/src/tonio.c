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
#include <wiringPi.h>
#include <stdbool.h>

#include "tonio.h"
#include "http.h"
#include "media.h"

int main(int argc, char** argv) {

    char *library_root = argv[1];
    MFRC522_Status_t ret;
    uint8_t ret_int;
    //Recognized card ID
    uint8_t card_id[5] = {0x00,};
    uint8_t selected_card_id[5] = {0x00,};

    uint8_t default_key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    struct timespec poll_interval = {
        .tv_sec = 0,
        .tv_nsec = 50/* msec */ * 1000000
    };

    openlog("tonio", LOG_PID | LOG_CONS | LOG_PERROR, LOG_USER);

    tn_media_t *media = tn_media_init(library_root);

    ret = MFRC522_Init(SPI_DEV, PIN_RFID, 'B');
    if (ret < 0) {
        syslog(LOG_CRIT, "RFID Failed to initialize");
        exit(EXIT_FAILURE);
    }
    syslog(LOG_INFO, "RFID reader successfully initialized");

    pinMode(PIN_PREV, INPUT);
    pinMode(PIN_NEXT, INPUT);
    pinMode(PIN_VOL_DOWN, INPUT);
    pinMode(PIN_VOL_UP, INPUT);

    pullUpDnControl(PIN_PREV, PUD_UP);
    pullUpDnControl(PIN_NEXT, PUD_UP);
    pullUpDnControl(PIN_VOL_DOWN, PUD_UP);
    pullUpDnControl(PIN_VOL_UP, PUD_UP);

    tn_http_t *http = tn_http_init(media, selected_card_id, library_root);

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

                int last_prev_state = PUD_OFF;
                int last_next_state = PUD_OFF;

                // Does some auth to make sure card still there.
                while (MFRC522_Auth((uint8_t) PICC_AUTHENT1A, (uint8_t) 0,
                        (uint8_t*) default_key, (uint8_t*) card_id) == MI_OK) {
                    nanosleep(&poll_interval, NULL);

                    int current_prev_state = digitalRead(PIN_PREV);
                    if (current_prev_state == PUD_DOWN && last_prev_state == PUD_OFF) {
                        tn_media_previous(media);
                    }
                    last_prev_state = current_prev_state;

                    int current_next_state = digitalRead(PIN_NEXT);
                    if (current_next_state == PUD_DOWN && last_next_state == PUD_OFF) {
                        tn_media_next(media);
                    }
                    last_next_state = current_next_state;

                    // volume kept continuous, more pleasant.
                    if (digitalRead(PIN_VOL_DOWN) == PUD_DOWN) tn_media_volume_down(media);
                    if (digitalRead(PIN_VOL_UP) == PUD_DOWN) tn_media_volume_up(media);

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

    closelog();

    return (EXIT_SUCCESS);
}

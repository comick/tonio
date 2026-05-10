/**
 * Copyright (c) 2020-2025 Michele Comignano <mcdev@playlinux.net>
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
#include <stdbool.h>
#include <confuse.h>
#include <signal.h>
#include <inttypes.h>

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

void _request_reload(int signal) {
    syslog(LOG_ALERT, "Reload requested.");
    // TODO use mutex or semaphore or whatever synchronized.
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

/**
 * Interactive main loop, for testing.
 * @param card_id
 * @param selected_card_id
 * @return
 */
static int _loop_interactive(uint8_t card_id[5], uint8_t selected_card_id[5]) {
    char line[128];

    while (true) {
        printf("tonio> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n");
            break;
        }

        /* Strip trailing newline. */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        if (len == 0) continue;

        /* Parse 4-byte hex UID. */
        unsigned int b0, b1, b2, b3;
        if (sscanf(line, "%2x%2x%2x%2x", &b0, &b1, &b2, &b3) != 4) {
            printf("Invalid card UID. Enter 8 hex digits (e.g., 0432A1B5).\n");
            continue;
        }

        card_id[0] = (uint8_t)b0;
        card_id[1] = (uint8_t)b1;
        card_id[2] = (uint8_t)b2;
        card_id[3] = (uint8_t)b3;
        card_id[4] = (uint8_t)(b0 ^ b1 ^ b2 ^ b3); /* XOR checksum */

        syslog(LOG_INFO, "Card uid detected (interactive): %02X%02X%02X%02X",
               card_id[0], card_id[1], card_id[2], card_id[3]);
        printf("\nCard detected: %02X%02X%02X%02X\n",
               card_id[0], card_id[1], card_id[2], card_id[3]);

        memcpy(&selected_card_id, &card_id, sizeof(*card_id));

        if (tn_media_play(media, card_id)) {
            /* Card is "present" — read commands or a new UID. */
            while (true) {
                printf("tonio:%02X%02X%02X%02X> ",
                       card_id[0], card_id[1], card_id[2], card_id[3]);
                fflush(stdout);

                if (fgets(line, sizeof(line), stdin) == NULL) {
                    printf("\n");
                    break;
                }

                size_t len = strlen(line);
                while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
                    line[--len] = '\0';

                if (len == 0 || strcmp(line, "q") == 0 ||
                    strcmp(line, "quit") == 0 || strcmp(line, "stop") == 0) {
                    /* Card removed. */
                    break;
                }

                if (strcmp(line, "n") == 0 || strcmp(line, "next") == 0) {
                    tn_media_next(media);
                    printf("-> next track\n");
                    continue;
                }
                if (strcmp(line, "p") == 0 || strcmp(line, "prev") == 0) {
                    tn_media_previous(media);
                    printf("-> previous track\n");
                    continue;
                }
                if (strcmp(line, "+") == 0 || strcmp(line, "volup") == 0) {
                    tn_media_volume_up(media);
                    printf("-> volume up\n");
                    continue;
                }
                if (strcmp(line, "-") == 0 || strcmp(line, "voldown") == 0) {
                    tn_media_volume_down(media);
                    printf("-> volume down\n");
                    continue;
                }
                if (strcmp(line, "s") == 0 || strcmp(line, "status") == 0) {
                    int cur = tn_media_track_current(media);
                    int tot = tn_media_track_total(media);
                    char *name = tn_media_track_name(media);
                    printf("track %d/%d%s%s\n",
                           cur + 1, tot,
                           name ? " - " : "",
                           name ? name : "");
                    free(name);
                    continue;
                }

                /* Try to parse as a new card UID to switch to. */
                unsigned int nb0, nb1, nb2, nb3;
                if (sscanf(line, "%2x%2x%2x%2x", &nb0, &nb1, &nb2, &nb3) == 4) {
                    tn_media_stop(media);
                    card_id[0] = (uint8_t)nb0;
                    card_id[1] = (uint8_t)nb1;
                    card_id[2] = (uint8_t)nb2;
                    card_id[3] = (uint8_t)nb3;
                    card_id[4] = (uint8_t)(nb0 ^ nb1 ^ nb2 ^ nb3);
                    printf("Switching to card: %02X%02X%02X%02X\n",
                           card_id[0], card_id[1], card_id[2], card_id[3]);
                    memcpy(&selected_card_id, &card_id, sizeof(*card_id));
                    tn_media_play(media, card_id);
                    continue;
                }

                printf("Commands:\n");
                printf("  <8-hex-digits>   switch to a new card\n");
                printf("  <empty>/q/stop   remove card / stop\n");
                printf("  n / next         next track\n");
                printf("  p / prev         previous track\n");
                printf("  + / volup        volume up\n");
                printf("  - / voldown      volume down\n");
                printf("  s / status       show current track\n");
            }

            memset(&selected_card_id, 0x00, sizeof(*selected_card_id));
            tn_media_stop(media);
            printf("Card removed.\n\n");
        }
    }

    _shutdown();
    return EXIT_SUCCESS;
}

/**
 * Main loop running on the device.
 * @param card_id
 * @param selected_card_id
 * @return exit status
 */
static int _loop(uint8_t card_id[5], uint8_t selected_card_id[5]) {
    /* Normal mode (RFID hardware) */
    while (true) {

        syslog(LOG_INFO, "Waiting for card...");

        while (!tn_input_tag_poll(input, card_id)) {
            _keep_alive();
        }

        syslog(LOG_INFO, "Card uid detected: %02" PRIX8 "%02" PRIX8 "%02" PRIX8 "%02" PRIX8, card_id[0], card_id[1], card_id[2], card_id[3]);

        if (!tn_input_tag_select(input, card_id)) {
            syslog(LOG_ERR, "Card select failed");
        } else {

            memcpy(&selected_card_id, &card_id, sizeof (*card_id));

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

            memset(&selected_card_id, 0x00, sizeof (*selected_card_id));
            tn_media_stop(media);

            syslog(LOG_INFO, "Card removed");

        }
    }

    return (EXIT_SUCCESS);
}

int main(int argc, char* argv[argc + 1]) {

    //Recognized card ID
    uint8_t card_id[5] = {0x00,};
    uint8_t selected_card_id[5] = {0x00,};
    bool interactive = false;

    openlog("tonio", LOG_PID | LOG_CONS | LOG_PERROR, LOG_USER);

    /* Parse flags before config argument. */
    int opt;
    while ((opt = getopt(argc, argv, "i")) != -1) {
        switch (opt) {
        case 'i':
            interactive = true;
            break;
        default:
            fprintf(stderr, "Usage: %s [-i] <config-file>\n", argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (optind >= argc) {
        syslog(LOG_CRIT, "Exactly one argument is expected, pointing to the configuration.");
        return EXIT_FAILURE;
    }

    const char *config_path = argv[optind];

    cfg = cfg_init(config_opts, CFGF_NONE);

    if (!cfg || cfg_parse(cfg, config_path) == CFG_PARSE_ERROR) {
        syslog(LOG_CRIT, "Cannot parse configuration at %s.", config_path);
        return EXIT_FAILURE;
    }

    media = tn_media_init(cfg);
    P_CHECK(media, return EXIT_FAILURE);

    if (!interactive) {
        input = tn_input_init(cfg);
    } else {
        syslog(LOG_INFO, "Running in interactive mode (no RFID hardware).");
        printf("Tonio interactive mode.\n");
        printf("Enter card UID as 8 hex digits (e.g., 0432A1B5).\n");
        printf("While playing: <empty>/q=stop  n=next  p=prev  +=volup  -=voldown\n");
        printf("\n");
    }

    http = tn_http_init(media, selected_card_id, cfg);
    P_CHECK(http, return EXIT_FAILURE);

    _argv = argv;

    signal(SIGUSR1, _request_reload);

    if (interactive) {
        return _loop_interactive(card_id, selected_card_id);
    }

    return _loop(card_id, selected_card_id);
}

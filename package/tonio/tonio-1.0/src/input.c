/**
 * Copyright (c) 2023 Michele Comignano <mcdev@playlinux.net>
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
#include <math.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <sys/time.h>
#include <ctype.h>
#include <string.h>
#include <sys/syslog.h>

#include "mfrc522.h"
#include "tonio.h"
#include "input.h"

const uint8_t default_key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

struct tn_input {
    struct gpiod_chip *gpio_chip;
    struct gpiod_line *gpio_mfrc522_line, *vol_up_line, *vol_down_line, *track_next_line, *track_prev_line;
};

tn_input_t *tn_input_init(cfg_t *cfg) {
    MFRC522_Status_t ret;
    tn_input_t *self = calloc(1, sizeof (tn_input_t));

    P_CHECK(self->gpio_chip = gpiod_chip_open_by_name(cfg_getstr(cfg, CFG_GPIOD_CHIP_NAME)), return NULL);
    P_CHECK(self->gpio_mfrc522_line = gpiod_chip_get_line(self->gpio_chip, cfg_getint(cfg, CFG_MFRC522_SWITCH)), return NULL);

    ret = MFRC522_Init(self->gpio_mfrc522_line, cfg_getstr(cfg, CFG_MFRC522_SPI_DEV), 'B');

    if (ret < 0) {
        syslog(LOG_CRIT, "RFID Failed to initialize");
        exit(EXIT_FAILURE);
    }
    syslog(LOG_INFO, "RFID reader successfully initialized");

    P_CHECK(self->track_next_line = gpiod_chip_get_line(self->gpio_chip, cfg_getint(cfg, CFG_BTN_TRACK_NEXT)), return NULL);
    P_CHECK(self->track_prev_line = gpiod_chip_get_line(self->gpio_chip, cfg_getint(cfg, CFG_BTN_TRACK_PREVIOUS)), return NULL);
    P_CHECK(self->vol_up_line = gpiod_chip_get_line(self->gpio_chip, cfg_getint(cfg, CFG_BTN_VOLUME_UP)), return NULL);
    P_CHECK(self->vol_down_line = gpiod_chip_get_line(self->gpio_chip, cfg_getint(cfg, CFG_BTN_VOLUME_DOWN)), return NULL);

    I_CHECK(gpiod_line_request_input(self->track_next_line, "tonio"), return NULL);
    I_CHECK(gpiod_line_request_input(self->track_prev_line, "tonio"), return NULL);
    I_CHECK(gpiod_line_request_input(self->vol_up_line, "tonio"), return NULL);
    I_CHECK(gpiod_line_request_input(self->vol_down_line, "tonio"), return NULL);

    return self;
}

bool tn_input_tag_poll(tn_input_t *self, uint8_t *card_id) {
    if (self == NULL) return false;

    return MFRC522_Check(card_id) == MI_OK;
}

bool tn_input_tag_check(tn_input_t *self, uint8_t *card_id) {
    if (self == NULL) return false;

    return MFRC522_Auth((uint8_t) PICC_AUTHENT1A, (uint8_t) 0,
            (uint8_t*) default_key, (uint8_t*) card_id) == MI_OK;
}

bool tn_input_tag_select(tn_input_t *self, uint8_t *card_id) {
    if (self == NULL) return false;

    uint8_t ret = MFRC522_SelectTag(card_id);
    syslog(LOG_INFO, "Card type: %s", MFRC522_TypeToString(MFRC522_ParseType(ret)));
    return ret == 0;
}

int tn_input_btn_next(tn_input_t *self) {
    if (self == NULL) return 0;

    return gpiod_line_get_value(self->track_next_line);
}

int tn_input_btn_prev(tn_input_t *self) {
    if (self == NULL) return 0;

    return gpiod_line_get_value(self->track_prev_line);
}

int tn_input_btn_vol_up(tn_input_t *self) {
    if (self == NULL) return 0;

    return gpiod_line_get_value(self->vol_up_line);
}

int tn_input_btn_vol_down(tn_input_t *self) {
    if (self == NULL) return 0;

    return gpiod_line_get_value(self->vol_down_line);
}

void tn_input_destroy(tn_input_t *self) {
    if (self == NULL) return;

    MFRC522_Halt();
    gpiod_chip_close(self->gpio_chip);
}
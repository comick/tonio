/**
 * Copyright (c) 2023-2024 Michele Comignano <mcdev@playlinux.net>
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
    struct gpiod_line_request *gpio_mfrc522_request;
    struct gpiod_line_request *btn_request;
    unsigned int track_next_off, track_prev_off, vol_up_off, vol_down_off;
};

tn_input_t *tn_input_init(cfg_t *cfg) {
    MFRC522_Status_t ret;
    tn_input_t *self = calloc(1, sizeof (tn_input_t));
    struct gpiod_line_settings *settings;
    struct gpiod_line_config *line_cfg;
    unsigned int mfrc522_off;

    P_CHECK(self->gpio_chip = gpiod_chip_open(cfg_getstr(cfg, CFG_GPIOD_CHIP_NAME)), return NULL);

    mfrc522_off = cfg_getint(cfg, CFG_MFRC522_SWITCH);
    self->track_next_off = cfg_getint(cfg, CFG_BTN_TRACK_NEXT);
    self->track_prev_off = cfg_getint(cfg, CFG_BTN_TRACK_PREVIOUS);
    self->vol_up_off = cfg_getint(cfg, CFG_BTN_VOLUME_UP);
    self->vol_down_off = cfg_getint(cfg, CFG_BTN_VOLUME_DOWN);

    settings = gpiod_line_settings_new();
    P_CHECK(settings, return NULL);
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_ACTIVE);

    line_cfg = gpiod_line_config_new();
    P_CHECK(line_cfg, { gpiod_line_settings_free(settings); return NULL; });
    I_CHECK(gpiod_line_config_add_line_settings(line_cfg, &mfrc522_off, 1, settings), {
        gpiod_line_settings_free(settings);
        gpiod_line_config_free(line_cfg);
        return NULL;
    });

    P_CHECK(self->gpio_mfrc522_request = gpiod_chip_request_lines(self->gpio_chip, NULL, line_cfg), {
        gpiod_line_settings_free(settings);
        gpiod_line_config_free(line_cfg);
        return NULL;
    });

    ret = MFRC522_Init(self->gpio_mfrc522_request, cfg_getstr(cfg, CFG_MFRC522_SPI_DEV), 'B');

    if (ret < 0) {
        syslog(LOG_CRIT, "RFID Failed to initialize");
        exit(EXIT_FAILURE);
    }
    syslog(LOG_INFO, "RFID reader successfully initialized");

    gpiod_line_settings_reset(settings);
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);

    gpiod_line_config_reset(line_cfg);
    unsigned int btn_offsets[] = {self->track_next_off, self->track_prev_off, self->vol_up_off, self->vol_down_off};
    I_CHECK(gpiod_line_config_add_line_settings(line_cfg, btn_offsets, 4, settings), {
        gpiod_line_settings_free(settings);
        gpiod_line_config_free(line_cfg);
        return NULL;
    });

    P_CHECK(self->btn_request = gpiod_chip_request_lines(self->gpio_chip, NULL, line_cfg), {
        gpiod_line_settings_free(settings);
        gpiod_line_config_free(line_cfg);
        return NULL;
    });

    gpiod_line_settings_free(settings);
    gpiod_line_config_free(line_cfg);

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
    return ret > 0;
}

int tn_input_btn_next(tn_input_t *self) {
    if (self == NULL) return 0;

    return gpiod_line_request_get_value(self->btn_request, self->track_next_off);
}

int tn_input_btn_prev(tn_input_t *self) {
    if (self == NULL) return 0;

    return gpiod_line_request_get_value(self->btn_request, self->track_prev_off);
}

int tn_input_btn_vol_up(tn_input_t *self) {
    if (self == NULL) return 0;

    return gpiod_line_request_get_value(self->btn_request, self->vol_up_off);
}

int tn_input_btn_vol_down(tn_input_t *self) {
    if (self == NULL) return 0;

    return gpiod_line_request_get_value(self->btn_request, self->vol_down_off);
}

void tn_input_destroy(tn_input_t *self) {
    if (self == NULL) return;
    
    syslog(LOG_INFO, "Shutting down input subsystem.");

    MFRC522_Halt();
    gpiod_line_request_release(self->gpio_mfrc522_request);
    gpiod_line_request_release(self->btn_request);
    gpiod_chip_close(self->gpio_chip);
}
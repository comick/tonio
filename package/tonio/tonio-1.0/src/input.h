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

#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>
#include <confuse.h>

typedef struct tn_input tn_input_t;

tn_input_t *tn_input_init(cfg_t *cfg);

bool tn_input_tag_poll(tn_input_t *, uint8_t *);
bool tn_input_tag_check(tn_input_t *, uint8_t *);
bool tn_input_tag_select(tn_input_t *, uint8_t *);

int tn_input_btn_next(tn_input_t *);
int tn_input_btn_prev(tn_input_t *);
int tn_input_btn_vol_up(tn_input_t *);
int tn_input_btn_vol_down(tn_input_t *);

void tn_input_destroy(tn_input_t *);

#endif /* INPUT_H */


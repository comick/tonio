/**
 * Copyright (c) 2020-2022 Michele Comignano <mcdev@playlinux.net>
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

#ifndef MEDIA_H
#define MEDIA_H

#include <stdbool.h>
#include <confuse.h>

#include "uthash.h"

#define DUMMY_AUDIO_OUT "dummy"

#define PLAYLIST_SUFFIX ".m3u"
#define PLAYLIST_SUFFIX_LEN (strlen(PLAYLIST_SUFFIX))

#define POS_SAVE_FILE_TEMPLATE ".tonio.dat.XXXXXX"
#define POS_SAVE_FILE ".tonio.dat"


/* Per-card saved position (hash table entry). */
typedef struct {
    uint32_t card_id;
    int media_idx;
    float media_pos;
    struct timeval time_saved;
    UT_hash_handle hh;
} tn_media_position_t;


typedef struct tn_media tn_media_t;

tn_media_t *tn_media_init(cfg_t *cfg);

bool tn_media_play(tn_media_t *, uint8_t *);
bool tn_media_is_playing(tn_media_t *);
int tn_media_track_current(tn_media_t *);
int tn_media_track_total(tn_media_t *);
char *tn_media_track_name(tn_media_t *);
void tn_media_next(tn_media_t *);
void tn_media_previous(tn_media_t *);
void tn_media_reset(tn_media_t *);
long tn_media_track_elapsed_ms(tn_media_t *);
void tn_media_stop(tn_media_t *);

void tn_media_volume_down(tn_media_t *);
void tn_media_volume_up(tn_media_t *);

void tn_media_destroy(tn_media_t *);

// Utils
char *find_playlist_filename(char *, uint8_t *);

#endif /* MEDIA_H */


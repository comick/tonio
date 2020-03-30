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

#include <stdlib.h>
#include <vlc/vlc.h>
#include <alsa/asoundlib.h>
#include <math.h>
#include <semaphore.h>
#include "uthash.h"

#include "volume_mapping.h"
#include "media.h"
#include "http.h"
#include "tonio.h"

#define AUDIO_OUTPUT "alsa"
#define MIXER_CARD "default"
#define MIXER_SELEM "PCM"
// 0 to 1.0 limits maximum volume.
#define VOL_MAX 0.5

static libvlc_instance_t * _vlc = NULL;
static libvlc_media_list_player_t *_media_list_player = NULL;
static libvlc_media_list_t *_media_list = NULL;
static char *_media_root = NULL;

static uint32_t curr_card_id;

typedef struct {
    uint32_t card_id;
    int media_idx;
    float media_pos;
    UT_hash_handle hh;
} tn_media_position_t;

static tn_media_position_t *_media_positions = NULL;

static snd_mixer_t *_mixer = NULL;
static snd_mixer_elem_t* _mixer_elem = NULL;

void tn_media_init(char *media_root) {
    // alsa mixer for volume
    snd_mixer_selem_id_t *selem_id = NULL;
    _media_root = media_root;

    // libvlc for playback
    const char *vlc_args[] = {
        "-I", "dummy", "--aout", AUDIO_OUTPUT
    };
    _vlc = libvlc_new(sizeof (vlc_args) / sizeof (vlc_args[0]), vlc_args);
    P_CHECK(_vlc, goto init_error);

    I_CHECK(snd_mixer_open(&_mixer, 0), goto init_error);

    I_CHECK(snd_mixer_attach(_mixer, MIXER_CARD), goto init_error);
    I_CHECK(snd_mixer_selem_register(_mixer, NULL, NULL), goto init_error);
    I_CHECK(snd_mixer_load(_mixer), goto init_error);

    snd_mixer_selem_id_alloca(&selem_id);
    snd_mixer_selem_id_set_index(selem_id, 0);
    snd_mixer_selem_id_set_name(selem_id, MIXER_SELEM);
    _mixer_elem = snd_mixer_find_selem(_mixer, selem_id);
    //snd_mixer_selem_id_free(&selem_id);
    P_CHECK(_mixer_elem, goto init_error);

    // effect is alsa volume to max configured
    tn_media_volume_up();

    syslog(LOG_INFO, "Media sub-system initialized");
    return;

init_error:
    syslog(LOG_EMERG, "Could not ininitalize media sub-system");
    tn_media_destroy();
    exit(EXIT_FAILURE);
}

static void _post_is_playing(const struct libvlc_event_t *event, void *user_data) {
    sem_t *is_playing = (sem_t *) user_data;
    sem_post(is_playing);
}

static char *_error_track_name = "error";

char *tn_media_track_name() {
    int current = tn_media_track_current();
    libvlc_media_t *current_media = libvlc_media_list_item_at_index(_media_list, current);
    P_CHECK(current_media, return &_error_track_name);

    char *name = libvlc_media_get_mrl(current_media);
    libvlc_media_release(current_media);
    return name;
}

bool tn_media_play(uint8_t *card_id) {
    bool played = false;
    libvlc_media_t *media = NULL;
    char *playlist_path = NULL;
    libvlc_media_player_t *media_player = NULL;


    if (_media_list_player != NULL) tn_media_stop();

    curr_card_id = card_id[0] << 24 | card_id[1] << 16 | card_id[2] << 8 | card_id[3];

    char *path_fmt = "%s/%02X%02X%02X%02X/%02X%02X%02X%02X.m3u";


    size_t path_len = snprintf(NULL, 0, path_fmt, _media_root,
            card_id[0], card_id[1], card_id[2], card_id[3],
            card_id[0], card_id[1], card_id[2], card_id[3]);
    playlist_path = malloc(path_len + 1);

    snprintf(playlist_path, path_len + 1, path_fmt, _media_root,
            card_id[0], card_id[1], card_id[2], card_id[3],
            card_id[0], card_id[1], card_id[2], card_id[3]);

    if (access(playlist_path, F_OK) < 0) {
        syslog(LOG_WARNING, "Cannot access '%s', setting up directoy stucture for new tag.\n", playlist_path);
        // TODO setup directory structure
        goto play_cleanup;
    }

    media = libvlc_media_new_path(_vlc, playlist_path);
    P_CHECK(media, goto play_cleanup);

    libvlc_media_parse_with_options(media,
            libvlc_media_parse_local | libvlc_media_parse_network,
            10000);

    _media_list = libvlc_media_subitems(media);
    P_CHECK(_media_list, goto play_cleanup);

    _media_list_player = libvlc_media_list_player_new(_vlc);
    P_CHECK(_media_list_player, goto play_cleanup);

    media_player = libvlc_media_player_new(_vlc);
    P_CHECK(media_player, goto play_cleanup);

    // set software volume to max, control via alsa mixer.
    libvlc_audio_set_volume(media_player, 100);

    libvlc_media_list_player_set_media_player(_media_list_player, media_player);
    libvlc_media_list_player_set_media_list(_media_list_player, _media_list);

    libvlc_media_list_player_set_playback_mode(_media_list_player, libvlc_playback_mode_loop);

    tn_media_position_t *remember_item = NULL;
    HASH_FIND_INT(_media_positions, &curr_card_id, remember_item);

    if (remember_item != NULL) {

        libvlc_event_manager_t *evt_manager = libvlc_media_player_event_manager(media_player);
        P_CHECK(evt_manager, goto play_cleanup);

        sem_t is_playing;
        I_CHECK(sem_init(&is_playing, 0, 0), goto play_cleanup);

        I_CHECK(libvlc_event_attach(evt_manager, libvlc_MediaPlayerPlaying, _post_is_playing, &is_playing), goto play_cleanup);

        I_CHECK(libvlc_media_list_player_play_item_at_index(_media_list_player, remember_item->media_idx), goto play_cleanup);

        sem_wait(&is_playing);

        libvlc_event_detach(evt_manager, libvlc_MediaPlayerPlaying, _post_is_playing, &is_playing);

        if (libvlc_media_player_is_seekable(media_player)) {

            syslog(LOG_INFO, "Recovering %s item %d (%f)", playlist_path, remember_item->media_idx, remember_item->media_pos);

            libvlc_media_player_set_position(media_player, remember_item->media_pos);

        } else {
            syslog(LOG_WARNING, "Media is not seekable, starting from the beginning");
        }

        sem_destroy(&is_playing);

    } else {
        syslog(LOG_INFO, "Start playing %s", playlist_path);
        libvlc_media_list_player_play(_media_list_player);
    }

    played = true;

play_cleanup:

    if (playlist_path != NULL) free(playlist_path);
    if (media_player != NULL) libvlc_media_player_release(media_player);
    if (media != NULL) libvlc_media_release(media);

    return played;
}

bool tn_media_is_playing() {
    return _media_list_player != NULL && libvlc_media_list_player_is_playing(_media_list_player);
}

int tn_media_track_current() {
    if (_media_list_player == NULL) return -1;

    libvlc_media_t *curr_media = NULL;
    libvlc_media_player_t *curr_media_player = NULL;

    curr_media_player = libvlc_media_list_player_get_media_player(_media_list_player);
    P_CHECK(curr_media_player, goto track_current_clean);

    curr_media = libvlc_media_player_get_media(curr_media_player);
    P_CHECK(curr_media, goto track_current_clean);

    return libvlc_media_list_index_of_item(_media_list, curr_media);

track_current_clean:
    if (curr_media_player != NULL) libvlc_media_player_release(curr_media_player);
    if (curr_media != NULL) libvlc_media_release(curr_media);
    return -1;
}

int tn_media_track_total() {
    if (_media_list == NULL) return -1;

    return libvlc_media_list_count(_media_list);

}

void tn_media_next(void) {
    if (_media_list_player == NULL) return;

    I_CHECK(libvlc_media_list_player_next(_media_list_player), NULL);


}

void tn_media_previous(void) {
    if (_media_list_player == NULL) return;

    libvlc_media_player_t *media_player = libvlc_media_list_player_get_media_player(_media_list_player);
    P_CHECK(media_player, return);
    libvlc_time_t time = libvlc_media_player_get_time(media_player);
    I_CHECK(time, return);

    // start current track if playing fro more than 2 secs
    if (time > 2000) libvlc_media_player_set_time(media_player, 0);
    else I_CHECK(libvlc_media_list_player_previous(_media_list_player), NULL);

    libvlc_media_player_release(media_player);
}

void tn_media_volume_down(void) {
    double vol_curr = get_normalized_playback_volume(_mixer_elem, SND_MIXER_SABSTRACT_BASIC);
    vol_curr = fmax(vol_curr - 0.05, 0.0);
    set_normalized_playback_volume(_mixer_elem, SND_MIXER_SABSTRACT_BASIC, vol_curr, 0);
}

void tn_media_volume_up(void) {
    double vol_curr = get_normalized_playback_volume(_mixer_elem, SND_MIXER_SABSTRACT_BASIC);
    vol_curr = fmin(vol_curr + 0.05, VOL_MAX);
    set_normalized_playback_volume(_mixer_elem, SND_MIXER_SABSTRACT_BASIC, vol_curr, 0);
}

void tn_media_stop(void) {

    libvlc_media_player_t *curr_media_player = NULL;
    libvlc_media_t *curr_media = NULL;
    int curr_i;
    float curr_pos;
    tn_media_position_t *curr_media_position = NULL;

    if (_media_list_player == NULL) return;

    // Saving current state for playing when left.
    curr_media_player = libvlc_media_list_player_get_media_player(_media_list_player);
    P_CHECK(curr_media_player, goto stop_clean);

    curr_media = libvlc_media_player_get_media(curr_media_player);
    P_CHECK(curr_media, goto stop_clean);

    curr_i = libvlc_media_list_index_of_item(_media_list, curr_media);
    I_CHECK(curr_i, goto stop_clean);

    curr_pos = libvlc_media_player_get_position(curr_media_player);

    curr_media_position = malloc(sizeof (tn_media_position_t));
    P_CHECK(curr_media_position, goto stop_clean);

    curr_media_position->card_id = curr_card_id;
    curr_media_position->media_idx = curr_i;
    curr_media_position->media_pos = curr_pos;
    HASH_ADD_INT(_media_positions, card_id, curr_media_position);

    syslog(LOG_INFO, "Stop playing item %d at position %f", curr_i, curr_pos);

stop_clean:
    if (curr_media_player != NULL) libvlc_media_player_release(curr_media_player);
    if (curr_media != NULL) libvlc_media_release(curr_media);
    if (_media_list != NULL) libvlc_media_list_release(_media_list);
    libvlc_media_list_player_stop(_media_list_player);
    libvlc_media_list_player_release(_media_list_player);
    _media_list_player = NULL;

}

void tn_media_destroy(void) {
    tn_media_stop();
    libvlc_release(_vlc);

    if (_mixer != NULL) {
        snd_mixer_close(_mixer);
        _mixer = NULL;
    }
}

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

#include <stdlib.h>
#include <vlc/vlc.h>
#include <alsa/asoundlib.h>
#include <math.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <sys/time.h>
#include <ctype.h>
#include <string.h>

#include "uthash.h"

#include "volume_mapping.h"
#include "media.h"
#include "http.h"
#include "tonio.h"

#define AUDIO_OUTPUT "alsa"

#define PLAYLIST_SUFFIX ".m3u"
#define PLAYLIST_SUFFIX_LEN (strlen(PLAYLIST_SUFFIX))

#define POS_SAVE_FILE_TEMPLATE ".tonio.dat.XXXXXX"
#define POS_SAVE_FILE ".tonio.dat"

typedef struct {
    uint32_t card_id;
    int media_idx;
    float media_pos;
    struct timeval time_saved;
    UT_hash_handle hh;
} tn_media_position_t;

struct tn_media {
    uint32_t curr_card_id;
    libvlc_instance_t * vlc;
    libvlc_media_list_player_t *media_list_player;
    libvlc_media_list_t *media_list;
    char *media_root;
    tn_media_position_t *media_positions;
    snd_mixer_t *mixer;
    snd_mixer_elem_t* mixer_elem;
    char *positions_filepath;
    char *tmp_positions_filepath_tpl;
    float volume_max;
    sem_t pos_file_mutex;
};

static void _save_event_detach(tn_media_t *);
static tn_media_position_t *_save_stream_positions(tn_media_t *);

tn_media_t *tn_media_init(cfg_t *cfg) {
    // alsa mixer for volume
    snd_mixer_selem_id_t *selem_id = NULL;

    tn_media_t *self = malloc(sizeof (tn_media_t));

    P_CHECK(self, goto init_error);

    self->curr_card_id = 0;
    self->vlc = NULL;
    self->media_list_player = NULL;
    self->media_list = NULL;
    self->media_root = cfg_getstr(cfg, CFG_MEDIA_ROOT);
    self->media_positions = NULL;
    self->mixer = NULL;
    self->mixer_elem = NULL;
    self->positions_filepath = NULL;
    self->volume_max = cfg_getfloat(cfg, CFG_VOLUME_MAX);

    sem_init(&self->pos_file_mutex, 0, 1);

    // libvlc for playback
    const char *vlc_args[] = {
        "-I", "dummy", "--aout", AUDIO_OUTPUT
    };
    self->vlc = libvlc_new(sizeof (vlc_args) / sizeof (vlc_args[0]), vlc_args);
    P_CHECK(self->vlc, goto init_error);

    I_CHECK(snd_mixer_open(&(self->mixer), 0), goto init_error);

    I_CHECK(snd_mixer_attach(self->mixer, cfg_getstr(cfg, CFG_MIXER_CARD)), goto init_error);
    I_CHECK(snd_mixer_selem_register(self->mixer, NULL, NULL), goto init_error);
    I_CHECK(snd_mixer_load(self->mixer), goto init_error);

    snd_mixer_selem_id_alloca(&selem_id);
    snd_mixer_selem_id_set_index(selem_id, 0);
    snd_mixer_selem_id_set_name(selem_id, cfg_getstr(cfg, CFG_MIXER_SELEM));
    self->mixer_elem = snd_mixer_find_selem(self->mixer, selem_id);
    //snd_mixer_selem_id_free(&selem_id);
    P_CHECK(self->mixer_elem, goto init_error);

    // effect is alsa volume to max configured
    tn_media_volume_up(self);

    // Loading stream positions from file.
    size_t buf_len = strlen(self->media_root) + strlen(POS_SAVE_FILE) + 1;
    self->positions_filepath = malloc(buf_len);
    strcpy(self->positions_filepath, self->media_root);
    strcat(self->positions_filepath, POS_SAVE_FILE);

    buf_len = strlen(self->media_root) + strlen(POS_SAVE_FILE_TEMPLATE) + 1;
    self->tmp_positions_filepath_tpl = malloc(buf_len);
    strcpy(self->tmp_positions_filepath_tpl, self->media_root);
    strcat(self->tmp_positions_filepath_tpl, POS_SAVE_FILE_TEMPLATE);

    FILE *positions_file = fopen(self->positions_filepath, "r");
    if (positions_file != NULL) {
        int i = 0;

        I_CHECK(fseek(positions_file, 0L, SEEK_END), goto init_error);
        long int size = ftell(positions_file);
        rewind(positions_file);

        while (ftell(positions_file) < size) {
            tn_media_position_t *saved_pos = malloc(sizeof (tn_media_position_t));
            ssize_t res = fread(&(saved_pos->card_id), sizeof (uint32_t), 1, positions_file);
            I_CHECK(res - 1, goto init_error);
            res = fread(&(saved_pos->media_idx), sizeof (int), 1, positions_file);
            I_CHECK(res - 1, goto init_error);
            res = fread(&(saved_pos->media_pos), sizeof (float), 1, positions_file);
            I_CHECK(res - 1, goto init_error);
            HASH_ADD_INT(self->media_positions, card_id, saved_pos);
            syslog(LOG_INFO, "Loaded playlist position for : %u @ %d : %f", saved_pos->card_id, saved_pos->media_idx, saved_pos->media_pos);
            i++;
        }
        fclose(positions_file);
        syslog(LOG_INFO, "Loaded %d stream positions from %s", i, self->positions_filepath);
    }

    syslog(LOG_INFO, "Media sub-system initialized");
    return self;

init_error:
    syslog(LOG_EMERG, "Could not ininitalize media sub-system");
    tn_media_destroy(self);
    return NULL;
}

static void _post_is_playing(const struct libvlc_event_t *event, void *user_data) {
    sem_t *is_playing = (sem_t *) user_data;
    sem_post(is_playing);
}

static char *_error_track_name = "error";

char *tn_media_track_name(tn_media_t *self) {
    int current = tn_media_track_current(self);
    libvlc_media_t *current_media = libvlc_media_list_item_at_index(self->media_list, current);
    P_CHECK(current_media, return _error_track_name);

    char *name = libvlc_media_get_mrl(current_media);
    libvlc_media_release(current_media);
    return name;
}

static int _apply_saved_position(tn_media_t *self, libvlc_media_player_t * media_player, char *tag_playlist_path, tn_media_position_t *saved_position) {

    I_CHECK(libvlc_media_list_player_play_item_at_index(self->media_list_player, saved_position->media_idx), return -1);

    if (libvlc_media_player_is_seekable(media_player)) {

        syslog(LOG_INFO, "Recovering %s item %d (%f)", tag_playlist_path, saved_position->media_idx, saved_position->media_pos);

        libvlc_media_player_set_position(media_player, saved_position->media_pos);

    } else {
        syslog(LOG_WARNING, "Media is not seekable, starting from the beginning");
    }

    return 0;
}

static void _save_stream_positions_onchange(const struct libvlc_event_t *event, void *data) {
    tn_media_t *self = (tn_media_t *) data;

    struct timeval now;
    I_CHECK(gettimeofday(&now, NULL), return);

    tn_media_position_t *media_pos = NULL;
    HASH_FIND_INT(self->media_positions, &(self->curr_card_id), media_pos);

    if (media_pos != NULL && now.tv_sec - media_pos->time_saved.tv_sec >= 2) {
        _save_stream_positions(self);
    }

}

bool tn_media_play(tn_media_t *self, uint8_t *card_id) {
    bool played = false;
    libvlc_media_t *media = NULL;
    char *tag_playlist_path = NULL;
    libvlc_media_player_t *media_player = NULL;
    sem_t is_playing;

    if (self->media_list_player != NULL) tn_media_stop(self);

    self->curr_card_id = card_id[0] << 24 | card_id[1] << 16 | card_id[2] << 8 | card_id[3];

    tag_playlist_path = find_playlist_filename(self->media_root, card_id);

    if (tag_playlist_path == NULL) {
        syslog(LOG_WARNING, "Cannot find playlist for %02X%02X%02X%02X.\n", card_id[0], card_id[1], card_id[2], card_id[3]);
        goto play_cleanup;
    }

    media = libvlc_media_new_path(self->vlc, tag_playlist_path);
    P_CHECK(media, goto play_cleanup);

    libvlc_media_parse_with_options(media,
            libvlc_media_parse_local | libvlc_media_parse_network,
            10000);

    self->media_list = libvlc_media_subitems(media);
    P_CHECK(self->media_list, goto play_cleanup);

    self->media_list_player = libvlc_media_list_player_new(self->vlc);
    P_CHECK(self->media_list_player, goto play_cleanup);

    media_player = libvlc_media_player_new(self->vlc);
    P_CHECK(media_player, goto play_cleanup);

    // set software volume to max, control via alsa mixer.
    libvlc_audio_set_volume(media_player, 100);

    libvlc_media_list_player_set_media_player(self->media_list_player, media_player);
    libvlc_media_list_player_set_media_list(self->media_list_player, self->media_list);

    libvlc_media_list_player_set_playback_mode(self->media_list_player, libvlc_playback_mode_loop);

    tn_media_position_t *saved_position = NULL;
    HASH_FIND_INT(self->media_positions, &(self->curr_card_id), saved_position);

    I_CHECK(sem_init(&is_playing, 0, 0), goto play_cleanup);
    libvlc_event_manager_t *evt_manager = libvlc_media_player_event_manager(media_player);
    I_CHECK(libvlc_event_attach(evt_manager, libvlc_MediaPlayerPlaying, _post_is_playing, &is_playing), goto play_cleanup);

    if (saved_position == NULL || !_apply_saved_position(self, media_player, tag_playlist_path, saved_position)) {
        syslog(LOG_INFO, "Start playing %s", tag_playlist_path);
        libvlc_media_list_player_play(self->media_list_player);
    }

    // Listening for position change won't work until playing.
    sem_wait(&is_playing);
    libvlc_event_detach(evt_manager, libvlc_MediaPlayerPlaying, _post_is_playing, &is_playing);
    // TODO fix, when playing card from beginning, event position changed is not received.
    I_CHECK(libvlc_event_attach(evt_manager, libvlc_MediaPlayerPositionChanged, _save_stream_positions_onchange, self), goto play_cleanup);

    sem_destroy(&is_playing);

    played = true;

play_cleanup:

    if (tag_playlist_path != NULL) free(tag_playlist_path);
    if (media_player != NULL) libvlc_media_player_release(media_player);
    if (media != NULL) libvlc_media_release(media);

    return played;
}

bool tn_media_is_playing(tn_media_t *self) {
    return self->media_list_player != NULL && libvlc_media_list_player_is_playing(self->media_list_player);
}

int tn_media_track_current(tn_media_t *self) {
    if (self->media_list_player == NULL) return -1;

    libvlc_media_t *curr_media = NULL;
    libvlc_media_player_t *curr_media_player = NULL;

    curr_media_player = libvlc_media_list_player_get_media_player(self->media_list_player);
    P_CHECK(curr_media_player, goto track_current_clean);

    curr_media = libvlc_media_player_get_media(curr_media_player);
    P_CHECK(curr_media, goto track_current_clean);

    return libvlc_media_list_index_of_item(self->media_list, curr_media);

track_current_clean:
    if (curr_media_player != NULL) libvlc_media_player_release(curr_media_player);
    if (curr_media != NULL) libvlc_media_release(curr_media);
    return -1;
}

int tn_media_track_total(tn_media_t *self) {
    if (self->media_list == NULL) return -1;

    return libvlc_media_list_count(self->media_list);

}

void tn_media_next(tn_media_t *self) {
    if (self->media_list_player == NULL) return;

    I_CHECK(libvlc_media_list_player_next(self->media_list_player), NULL);
    _save_stream_positions(self);
}

void tn_media_previous(tn_media_t *self) {
    if (self->media_list_player == NULL) return;

    libvlc_media_player_t *media_player = libvlc_media_list_player_get_media_player(self->media_list_player);
    P_CHECK(media_player, return);
    libvlc_time_t time = libvlc_media_player_get_time(media_player);
    I_CHECK(time, return);

    // start current track if playing for more than 2 secs
    if (time > 2000) libvlc_media_player_set_time(media_player, 0);
    else I_CHECK(libvlc_media_list_player_previous(self->media_list_player), NULL);
    _save_stream_positions(self);

    libvlc_media_player_release(media_player);
}

void tn_media_volume_down(tn_media_t *self) {
    double vol_curr = get_normalized_playback_volume(self->mixer_elem, SND_MIXER_SCHN_UNKNOWN);
    vol_curr = fmax(vol_curr - 0.05, 0.0);
    set_normalized_playback_volume(self->mixer_elem, SND_MIXER_SCHN_UNKNOWN, vol_curr, 0);
}

void tn_media_volume_up(tn_media_t *self) {
    double vol_curr = get_normalized_playback_volume(self->mixer_elem, SND_MIXER_SCHN_UNKNOWN);
    vol_curr = fmin(vol_curr + 0.05, self->volume_max);
    set_normalized_playback_volume(self->mixer_elem, SND_MIXER_SCHN_UNKNOWN, vol_curr, 0);
}

void tn_media_stop(tn_media_t *self) {
    if (self->media_list_player == NULL) return;

    _save_event_detach(self);

    tn_media_position_t *curr_pos = _save_stream_positions(self);

    if (self->media_list != NULL) libvlc_media_list_release(self->media_list);
    self->media_list = NULL;

    libvlc_media_list_player_stop(self->media_list_player);
    libvlc_media_list_player_release(self->media_list_player);
    self->media_list_player = NULL;

    self->curr_card_id = 0;

    syslog(LOG_INFO, "Stop playing item %d at position %f", curr_pos->media_idx, curr_pos->media_pos);
}

void _save_event_detach(tn_media_t *self) {
    libvlc_media_player_t *curr_media_player = NULL;
    // Save stream position in memory.
    curr_media_player = libvlc_media_list_player_get_media_player(self->media_list_player);
    P_CHECK(curr_media_player, return);
    libvlc_event_manager_t *evt_manager = libvlc_media_player_event_manager(curr_media_player);
    libvlc_event_detach(evt_manager, libvlc_MediaPlayerPositionChanged, _save_stream_positions_onchange, self);
}

static tn_media_position_t *_save_stream_positions(tn_media_t *self) {

    libvlc_media_player_t *curr_media_player = NULL;
    libvlc_media_t *curr_media = NULL;
    int tmp_fd = -1;
    int curr_i;
    float curr_pos;
    char *tmp_file_name = NULL;
    int i;

    tn_media_position_t *media_pos = NULL;

    // Save stream position in memory.
    curr_media_player = libvlc_media_list_player_get_media_player(self->media_list_player);
    P_CHECK(curr_media_player, goto save_pos_clean);

    curr_media = libvlc_media_player_get_media(curr_media_player);
    P_CHECK(curr_media, goto save_pos_clean);

    curr_i = libvlc_media_list_index_of_item(self->media_list, curr_media);
    I_CHECK(curr_i, goto save_pos_clean);

    curr_pos = libvlc_media_player_get_position(curr_media_player);

    HASH_FIND_INT(self->media_positions, &(self->curr_card_id), media_pos);

    if (media_pos == NULL) {
        media_pos = malloc(sizeof (tn_media_position_t));
        P_CHECK(media_pos, goto save_pos_clean);

        media_pos->card_id = self->curr_card_id;
        HASH_ADD_INT(self->media_positions, card_id, media_pos);
    }
    I_CHECK(gettimeofday(&media_pos->time_saved, NULL), goto save_pos_clean);
    media_pos->media_idx = curr_i;
    media_pos->media_pos = curr_pos;

    tmp_file_name = malloc(strlen(self->tmp_positions_filepath_tpl) + 1);
    tmp_file_name = strcpy(tmp_file_name, self->tmp_positions_filepath_tpl);

    tmp_fd = mkstemp(tmp_file_name);
    I_CHECK(tmp_fd, goto save_pos_clean);

    FILE *positions_file = fdopen(tmp_fd, "w");
    P_CHECK(positions_file, syslog(LOG_ERR, "Cannot write positions file: %s", self->positions_filepath); goto save_pos_clean);
    tn_media_position_t * nxt = self->media_positions;
    for (i = 0; nxt != NULL; nxt = nxt->hh.next, i++) {
        fwrite(&(nxt->card_id), sizeof (uint32_t), 1, positions_file);
        fwrite(&(nxt->media_idx), sizeof (int), 1, positions_file);
        fwrite(&(nxt->media_pos), sizeof (float), 1, positions_file);

        syslog(LOG_DEBUG, "Saved playlist position for: %u @ %d : %f", nxt->card_id, nxt->media_idx, nxt->media_pos);
    }
    fflush(positions_file);
    fclose(positions_file);

    sem_wait(&self->pos_file_mutex);
    I_CHECK(rename(tmp_file_name, self->positions_filepath), goto save_pos_clean);
    sem_post(&self->pos_file_mutex);

    syslog(LOG_INFO, "Saved %d stream positions at %s", i, self->positions_filepath);

save_pos_clean:
    if (curr_media_player != NULL) libvlc_media_player_release(curr_media_player);
    if (curr_media != NULL) libvlc_media_release(curr_media);
    if (tmp_file_name != NULL) unlink(tmp_file_name);

    return media_pos;
}

void tn_media_destroy(tn_media_t *self) {
    tn_media_stop(self);
    libvlc_release(self->vlc);

    if (self->mixer != NULL) {
        snd_mixer_close(self->mixer);
        self->mixer = NULL;
    }

    free(self);
}

/**
 * Finds the first playlist file and returns.
 * Result must be freed by caller.
 */
char *find_playlist_filename(char *media_root, uint8_t *card_id) {

    char *tag_library_path = NULL;
    char *tag_playlist_path = NULL;
    char *path_fmt = "%s/%02X%02X%02X%02X";
    char *playlist_path_fmt = "%s/%s";


    size_t path_len = snprintf(NULL, 0, path_fmt, media_root,
            card_id[0], card_id[1], card_id[2], card_id[3]);
    tag_library_path = malloc(path_len + 1);

    snprintf(tag_library_path, path_len + 1, path_fmt, media_root,
            card_id[0], card_id[1], card_id[2], card_id[3]);

    // Search for first m3u file.
    DIR *dir = opendir(tag_library_path);
    P_CHECK(dir, goto find_cleanup);
    struct dirent *ent = readdir(dir);
    while (ent != NULL) {
        size_t dir_len = strlen(ent->d_name);
        char *dir_suffix = ent->d_name + dir_len - PLAYLIST_SUFFIX_LEN;
        if (dir_len > PLAYLIST_SUFFIX_LEN && strcmp(dir_suffix, PLAYLIST_SUFFIX) == 0) {
            size_t playlist_path_len = snprintf(NULL, 0, playlist_path_fmt, tag_library_path, ent->d_name);
            tag_playlist_path = malloc(playlist_path_len + 1);
            snprintf(tag_playlist_path, playlist_path_len + 1, playlist_path_fmt, tag_library_path, ent->d_name);
            break;
        }
        ent = readdir(dir);
    }

find_cleanup:
    if (tag_library_path != NULL) free(tag_library_path);

    return tag_playlist_path;
}

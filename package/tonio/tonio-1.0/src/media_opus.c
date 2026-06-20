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
 *
 * Opusfile-based media backend. Replaces libvlc with opusfile + ALSA PCM
 * for Opus audio playback. Selected at compile time via --enable-media=opus
 * (the default).
 */

#include "config.h"

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <semaphore.h>
#include <pthread.h>
#include <dirent.h>
#include <pwd.h>
#include <inttypes.h>
#include <sys/syslog.h>
#include <alsa/asoundlib.h>
#include <opus/opus.h>
#include <opus/opusfile.h>

#include "volume_mapping.h"
#include "media.h"
#include "http.h"
#include "tonio.h"

/* PCM device name is taken from media-audio-out config at runtime */
#define PCM_FORMAT SND_PCM_FORMAT_S16_LE
#define PCM_CHANNELS 2
#define PCM_RATE 48000
#define PCM_PERIOD_FRAMES 1024
#define PCM_BUFFER_FRAMES (PCM_PERIOD_FRAMES * 4)

#define PCM_BUF_SAMPLES (PCM_PERIOD_FRAMES * PCM_CHANNELS)

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif


typedef struct tn_media {
    uint32_t curr_card_id;
    char *media_root;
    tn_media_position_t *media_positions;
    snd_mixer_t *mixer;
    snd_mixer_elem_t *mixer_elem;
    char *positions_filepath;
    char *tmp_positions_filepath_tpl;
    char *audio_out;
    float volume_max;
    sem_t pos_file_mutex;

    /* Playback thread and synchronization */
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    volatile bool thread_running;
    volatile bool stop_requested;
    volatile bool next_requested;
    volatile bool prev_requested;
    volatile bool play_requested;
    volatile bool reset_requested;
    struct timeval track_start_time;
    uint8_t pending_card_id[4];

    /* Current playlist / track state (owned by playback thread) */
    char **tracks;
    int track_count;
    int current_track;
    bool is_playing;
} tn_media_t;

/* Forward declarations */
static int _open_pcm_playback(snd_pcm_t **pcm, const char *device);

static int _parse_playlist(const char *playlist_path,
                           char ***tracks_out, int *count_out);

static void *_playback_thread(void *arg);

static tn_media_position_t *_save_stream_positions(tn_media_t *self,
                                                   int track_idx, float pos);

static char *_error_track_name = "error";


tn_media_t *tn_media_init(cfg_t *cfg) {
    tn_media_t *self = calloc(1, sizeof(tn_media_t));
    P_CHECK(self, goto init_error);

    self->media_root = cfg_getstr(cfg, CFG_MEDIA_ROOT);
    self->volume_max = cfg_getfloat(cfg, CFG_VOLUME_MAX);
    self->audio_out = cfg_getstr(cfg, CFG_MEDIA_AUDIO_OUT);
    self->curr_card_id = 0;

    sem_init(&self->pos_file_mutex, 0, 1);
    pthread_mutex_init(&self->mutex, NULL);
    pthread_cond_init(&self->cond, NULL);

    /* Open ALSA mixer for volume control (skip in dummy mode). */
    if (strncmp(self->audio_out, DUMMY_AUDIO_OUT,
                strlen(DUMMY_AUDIO_OUT)) != 0) {
        I_CHECK(snd_mixer_open(&(self->mixer), 0), goto init_error);
        I_CHECK(snd_mixer_attach(self->mixer,
                    cfg_getstr(cfg, CFG_MIXER_CARD)),
                goto init_error);
        I_CHECK(snd_mixer_selem_register(self->mixer, NULL, NULL),
                goto init_error);
        I_CHECK(snd_mixer_load(self->mixer), goto init_error);

        snd_mixer_selem_id_t *selem_id = NULL;
        snd_mixer_selem_id_alloca(&selem_id);
        snd_mixer_selem_id_set_index(selem_id, 0);
        snd_mixer_selem_id_set_name(selem_id,
                                    cfg_getstr(cfg, CFG_MIXER_SELEM));
        self->mixer_elem = snd_mixer_find_selem(self->mixer, selem_id);
        P_CHECK(self->mixer_elem, goto init_error);
    }

    /* Set ALSA volume to configured max. */
    tn_media_volume_up(self);

    /* Load saved stream positions from file */
    size_t buf_len = strlen(self->media_root) + strlen(POS_SAVE_FILE) + 1;
    self->positions_filepath = malloc(buf_len);
    strcpy(self->positions_filepath, self->media_root);
    strcat(self->positions_filepath, POS_SAVE_FILE);

    buf_len = strlen(self->media_root) + strlen(POS_SAVE_FILE_TEMPLATE) + 1;
    self->tmp_positions_filepath_tpl = malloc(buf_len);
    strcpy(self->tmp_positions_filepath_tpl, self->media_root);
    strcat(self->tmp_positions_filepath_tpl, POS_SAVE_FILE_TEMPLATE);

    FILE *pf = fopen(self->positions_filepath, "r");
    if (pf != NULL) {
        uint32_t i = 0;
        I_CHECK(fseek(pf, 0L, SEEK_END), goto init_error);
        long int size = ftell(pf);
        rewind(pf);

        while (ftell(pf) < size) {
            tn_media_position_t *sp = malloc(sizeof(tn_media_position_t));
            ssize_t res = fread(&(sp->card_id), sizeof(uint32_t), 1, pf);
            I_CHECK(res - 1, goto init_error);
            res = fread(&(sp->media_idx), sizeof(int), 1, pf);
            I_CHECK(res - 1, goto init_error);
            res = fread(&(sp->media_pos), sizeof(float), 1, pf);
            I_CHECK(res - 1, goto init_error);
            HASH_ADD_INT(self->media_positions, card_id, sp);
            syslog(LOG_INFO, "Loaded pos: %" PRIu32 "@%d: %f",
                   sp->card_id, sp->media_idx, sp->media_pos);
            i++;
        }
        fclose(pf);
        syslog(LOG_INFO, "Loaded %" PRIu32 " saved positions", i);
    }

    syslog(LOG_INFO, "Media subsystem initialized (opusfile): %s",
           self->audio_out);
    return self;

init_error:
    syslog(LOG_EMERG, "Could not initialize media subsystem");
    tn_media_destroy(self);
    return NULL;
}

/* Track info */

char *tn_media_track_name(tn_media_t *self) {
    if (!self->is_playing || self->tracks == NULL) return NULL;
    return strdup(self->tracks[self->current_track]);
}

int tn_media_track_current(tn_media_t *self) {
    if (!self->is_playing) return -1;
    return self->current_track;
}

int tn_media_track_total(tn_media_t *self) {
    return self->track_count;
}

bool tn_media_is_playing(tn_media_t *self) {
    return self->is_playing;
}

/* Playback control */

bool tn_media_play(tn_media_t *self, uint8_t *card_id) {
    if (self->is_playing) tn_media_stop(self);

    self->curr_card_id = ((uint32_t) card_id[0] << 24) |
                         ((uint32_t) card_id[1] << 16) |
                         ((uint32_t) card_id[2] << 8) |
                         ((uint32_t) card_id[3]);

    memcpy(self->pending_card_id, card_id, 4);

    pthread_mutex_lock(&self->mutex);
    self->play_requested = true;
    self->stop_requested = false;
    self->next_requested = false;
    self->prev_requested = false;
    self->reset_requested = false;

    if (!self->thread_running) {
        int ret = pthread_create(&self->thread, NULL,
                                 _playback_thread, self);
        if (ret != 0) {
            syslog(LOG_ERR, "pthread_create failed: %s", strerror(ret));
            pthread_mutex_unlock(&self->mutex);
            return false;
        }
        self->thread_running = true;
    } else {
        pthread_cond_signal(&self->cond);
    }
    pthread_mutex_unlock(&self->mutex);

    syslog(LOG_INFO, "Playing card %02X%02X%02X%02X",
           card_id[0], card_id[1], card_id[2], card_id[3]);
    return true;
}

void tn_media_stop(tn_media_t *self) {
    if (!self->is_playing && !self->thread_running) return;

    pthread_mutex_lock(&self->mutex);
    self->stop_requested = true;
    self->play_requested = false;
    pthread_cond_signal(&self->cond);
    pthread_mutex_unlock(&self->mutex);

    if (self->thread_running) {
        pthread_join(self->thread, NULL);
        self->thread_running = false;
    }

    /* Free playlist. */
    if (self->tracks != NULL) {
        for (int i = 0; i < self->track_count; i++) free(self->tracks[i]);
        free(self->tracks);
        self->tracks = NULL;
    }
    self->track_count = 0;
    self->current_track = 0;
    self->is_playing = false;
    self->curr_card_id = 0;

    syslog(LOG_INFO, "Playback stopped");
}

void tn_media_next(tn_media_t *self) {
    if (!self->is_playing) return;
    pthread_mutex_lock(&self->mutex);
    self->next_requested = true;
    pthread_cond_signal(&self->cond);
    pthread_mutex_unlock(&self->mutex);
}

void tn_media_previous(tn_media_t *self) {
    if (!self->is_playing) return;
    pthread_mutex_lock(&self->mutex);
    self->prev_requested = true;
    pthread_cond_signal(&self->cond);
    pthread_mutex_unlock(&self->mutex);
}

void tn_media_reset(tn_media_t *self) {
    if (!self->is_playing) return;
    pthread_mutex_lock(&self->mutex);
    self->reset_requested = true;
    pthread_cond_signal(&self->cond);
    pthread_mutex_unlock(&self->mutex);
}

long tn_media_track_elapsed_ms(tn_media_t *self) {
    if (!self->is_playing) return 0;
    struct timeval now, tv;
    pthread_mutex_lock(&self->mutex);
    tv = self->track_start_time;
    pthread_mutex_unlock(&self->mutex);
    gettimeofday(&now, NULL);
    return (now.tv_sec - tv.tv_sec) * 1000 +
           (now.tv_usec - tv.tv_usec) / 1000;
}

/* Volume control */

void tn_media_volume_down(tn_media_t *self) {
    if (self->mixer_elem == NULL) return;
    double v = get_normalized_playback_volume(self->mixer_elem,
                                              SND_MIXER_SCHN_UNKNOWN);
    v = fmax(v - 0.05, 0.0);
    set_normalized_playback_volume(self->mixer_elem, SND_MIXER_SCHN_UNKNOWN,
                                   v, 0);
}

void tn_media_volume_up(tn_media_t *self) {
    if (self->mixer_elem == NULL) return;
    double v = get_normalized_playback_volume(self->mixer_elem,
                                              SND_MIXER_SCHN_UNKNOWN);
    v = fmin(v + 0.05, self->volume_max);
    set_normalized_playback_volume(self->mixer_elem, SND_MIXER_SCHN_UNKNOWN,
                                   v, 0);
}

/* Destroy */

void tn_media_destroy(tn_media_t *self) {
    syslog(LOG_INFO, "Shutting down media subsystem.");
    tn_media_stop(self);

    if (self->mixer != NULL) {
        snd_mixer_close(self->mixer);
        self->mixer = NULL;
    }

    free(self->positions_filepath);
    free(self->tmp_positions_filepath_tpl);

    pthread_mutex_destroy(&self->mutex);
    pthread_cond_destroy(&self->cond);
    sem_destroy(&self->pos_file_mutex);

    tn_media_position_t *pos, *tmp;
    HASH_ITER(hh, self->media_positions, pos, tmp) {
        HASH_DEL(self->media_positions, pos);
        free(pos);
    }

    free(self);
}


/* Position save / restore */

static tn_media_position_t *_save_stream_positions(tn_media_t *self,
                                                   int track_idx,
                                                   float pos) {
    tn_media_position_t *mp = NULL;
    HASH_FIND_INT(self->media_positions, &(self->curr_card_id), mp);

    if (mp == NULL) {
        mp = malloc(sizeof(tn_media_position_t));
        P_CHECK(mp, return NULL);
        mp->card_id = self->curr_card_id;
        HASH_ADD_INT(self->media_positions, card_id, mp);
    }

    gettimeofday(&mp->time_saved, NULL);
    mp->media_idx = track_idx;
    mp->media_pos = pos;

    /* Write all positions atomically to a temp file, then rename. */
    char *tmp_name = strdup(self->tmp_positions_filepath_tpl);
    int tmp_fd = mkstemp(tmp_name);
    I_CHECK(tmp_fd, { free(tmp_name); return mp; });

    FILE *pf = fdopen(tmp_fd, "w");
    if (pf == NULL) {
        syslog(LOG_ERR, "Cannot write positions: %s",
               self->positions_filepath);
        close(tmp_fd);
        unlink(tmp_name);
        free(tmp_name);
        return mp;
    }

    size_t count = 0;
    for (tn_media_position_t *n = self->media_positions;
         n != NULL; n = n->hh.next, count++) {
        fwrite(&(n->card_id), sizeof(uint32_t), 1, pf);
        fwrite(&(n->media_idx), sizeof(int), 1, pf);
        fwrite(&(n->media_pos), sizeof(float), 1, pf);
    }
    fflush(pf);
    fclose(pf);

    sem_wait(&self->pos_file_mutex);
    rename(tmp_name, self->positions_filepath);
    sem_post(&self->pos_file_mutex);

    unlink(tmp_name);
    free(tmp_name);

    syslog(LOG_DEBUG, "Saved %zu positions", count);
    return mp;
}

/* ALSA PCM device setup */

static int _open_pcm_playback(snd_pcm_t **pcm, const char *device) {
    int rc;
    snd_pcm_hw_params_t *hw;

    rc = snd_pcm_open(pcm, device, SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) {
        syslog(LOG_ERR, "Cannot open PCM '%s': %s",
               device, snd_strerror(rc));
        return rc;
    }

    snd_pcm_hw_params_alloca(&hw);
    rc = snd_pcm_hw_params_any(*pcm, hw);
    if (rc < 0) goto fail;

    rc = snd_pcm_hw_params_set_access(*pcm, hw,
                                      SND_PCM_ACCESS_RW_INTERLEAVED);
    if (rc < 0) goto fail;

    rc = snd_pcm_hw_params_set_format(*pcm, hw, PCM_FORMAT);
    if (rc < 0) goto fail;

    unsigned int rate = PCM_RATE;
    rc = snd_pcm_hw_params_set_rate_near(*pcm, hw, &rate, NULL);
    if (rc < 0) goto fail;

    rc = snd_pcm_hw_params_set_channels(*pcm, hw, PCM_CHANNELS);
    if (rc < 0) goto fail;

    snd_pcm_uframes_t ps = PCM_PERIOD_FRAMES;
    rc = snd_pcm_hw_params_set_period_size_near(*pcm, hw, &ps, NULL);
    if (rc < 0) goto fail;

    snd_pcm_uframes_t bs = PCM_BUFFER_FRAMES;
    rc = snd_pcm_hw_params_set_buffer_size_near(*pcm, hw, &bs);
    if (rc < 0) goto fail;

    rc = snd_pcm_hw_params(*pcm, hw);
    if (rc < 0) goto fail;

    rc = snd_pcm_prepare(*pcm);
    if (rc < 0) goto fail;

    return 0;

fail:
    syslog(LOG_ERR, "PCM setup failed: %s", snd_strerror(rc));
    snd_pcm_close(*pcm);
    *pcm = NULL;
    return rc;
}

/* M3U playlist parser */

static int _parse_playlist(const char *playlist_path,
                           char ***tracks_out, int *count_out) {
    FILE *fp = fopen(playlist_path, "r");
    if (fp == NULL) {
        syslog(LOG_ERR, "Cannot open playlist '%s': %s",
               playlist_path, strerror(errno));
        return -1;
    }

    /* Base directory of the playlist file. */
    char *base_dir = strdup(playlist_path);
    char *slash = strrchr(base_dir, '/');
    if (slash != NULL) {
        *slash = '\0';
    } else {
        free(base_dir);
        base_dir = strdup(".");
    }

    int capacity = 0, count = 0;
    char **tracks = NULL;
    char line[4096];

    while (fgets(line, sizeof(line), fp) != NULL) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len == 0 || line[0] == '#') continue;

        /* Build full path (relative entries are resolved against the
         * playlist's directory, absolute paths are kept as-is). */
        char *full;
        if (line[0] == '/') {
            full = strdup(line);
        } else {
            size_t plen = snprintf(NULL, 0, "%s/%s", base_dir, line);
            full = malloc(plen + 1);
            snprintf(full, plen + 1, "%s/%s", base_dir, line);
        }

        if (access(full, R_OK) != 0) {
            syslog(LOG_WARNING, "Skipping inaccessible track: %s", full);
            free(full);
            continue;
        }

        if (count >= capacity) {
            capacity = capacity ? capacity * 2 : 16;
            char **nt = realloc(tracks, capacity * sizeof(char *));
            if (nt == NULL) {
                for (int i = 0; i < count; i++) free(tracks[i]);
                free(tracks);
                free(base_dir);
                fclose(fp);
                return -1;
            }
            tracks = nt;
        }
        tracks[count++] = full;
    }

    fclose(fp);
    free(base_dir);
    *tracks_out = tracks;
    *count_out = count;
    syslog(LOG_INFO, "Parsed %d tracks from '%s'", count, playlist_path);
    return 0;
}

/* PCM offset <-> normalised position converters */

static ogg_int64_t _pos_to_pcm_offset(OggOpusFile *of, float pos) {
    ogg_int64_t total = op_pcm_total(of, -1);
    return (total > 0) ? (ogg_int64_t) (pos * (double) total) : 0;
}

static float _pcm_offset_to_pos(OggOpusFile *of, ogg_int64_t off) {
    ogg_int64_t total = op_pcm_total(of, -1);
    return (total > 0) ? (float) ((double) off / (double) total) : 0.0f;
}

/* Convert mono PCM buffer to stereo in-place */

static void _mono_to_stereo(opus_int16 *buf, int frames) {
    /* Work from the end backwards to allow in-place conversion. */
    for (int i = frames - 1; i >= 0; i--) {
        buf[i * 2 + 1] = buf[i];
        buf[i * 2] = buf[i];
    }
}

// PLayback thread

static void *_playback_thread(void *arg) {
    tn_media_t *self = (tn_media_t *) arg;
    bool is_dummy = (strncmp(self->audio_out, DUMMY_AUDIO_OUT,
                             strlen(DUMMY_AUDIO_OUT)) == 0);

    snd_pcm_t *pcm = NULL;
    opus_int16 pcm_buf[PCM_BUF_SAMPLES * 2]; /* extra room for mono→stereo */

    while (1) {
        /* Wait for a play request (or exit). */
        pthread_mutex_lock(&self->mutex);
        while (!self->play_requested && !self->stop_requested)
            pthread_cond_wait(&self->cond, &self->mutex);

        if (self->stop_requested) {
            self->thread_running = false;
            pthread_mutex_unlock(&self->mutex);
            break;
        }

        self->play_requested = false;
        uint8_t card_id[4];
        memcpy(card_id, self->pending_card_id, 4);
        pthread_mutex_unlock(&self->mutex);

        /* Find and parse playlist */
        char *pl_path = find_playlist_filename(self->media_root, card_id);
        if (pl_path == NULL) {
            syslog(LOG_WARNING, "No playlist for %02X%02X%02X%02X",
                   card_id[0], card_id[1], card_id[2], card_id[3]);
            continue;
        }

        char **tracks = NULL;
        int track_count = 0;
        if (_parse_playlist(pl_path, &tracks, &track_count) < 0 ||
            track_count == 0) {
            syslog(LOG_ERR, "No playable tracks in '%s'", pl_path);
            free(pl_path);
            continue;
        }
        free(pl_path);

        pthread_mutex_lock(&self->mutex);
        if (self->tracks != NULL) {
            for (int i = 0; i < self->track_count; i++) free(self->tracks[i]);
            free(self->tracks);
        }
        self->tracks = tracks;
        self->track_count = track_count;
        self->current_track = 0;
        pthread_mutex_unlock(&self->mutex);

        /* Restore saved position (if any). */
        int start_track = 0;
        float start_pos = 0.0f;
        tn_media_position_t *saved = NULL;
        HASH_FIND_INT(self->media_positions, &(self->curr_card_id), saved);
        if (saved != NULL && saved->media_idx < track_count) {
            start_track = saved->media_idx;
            start_pos = saved->media_pos;
            syslog(LOG_INFO, "Resume %" PRIu32 " @ track %d (%.2f%%)",
                   self->curr_card_id, start_track, start_pos * 100.0f);
        }

        /* Open PCM device */
        const char *pcm_device = (strcmp(self->audio_out, "alsa") == 0)
                                     ? "default"
                                     : self->audio_out;
        if (!is_dummy && _open_pcm_playback(&pcm, pcm_device) < 0) {
            syslog(LOG_ERR, "Cannot open PCM, aborting");
            goto cleanup_tracks;
        }

        self->is_playing = true;
        syslog(LOG_INFO, "Playback started (%d tracks)", track_count);

        /* Main decode/output loop */
        int track_idx = start_track;
        bool first_track = true;

        while (1) {
            /* Check commands under mutex. */
            pthread_mutex_lock(&self->mutex);
            if (self->stop_requested) {
                pthread_mutex_unlock(&self->mutex);
                goto done;
            }
            if (self->next_requested) {
                self->next_requested = false;
                self->current_track = track_idx;
                pthread_mutex_unlock(&self->mutex);

                /* Save position at the point of skip. */
                _save_stream_positions(self, track_idx, 0.0f);
                track_idx = (track_idx + 1) % track_count;
                first_track = false;
                continue;
            }
            if (self->prev_requested) {
                self->prev_requested = false;
                pthread_mutex_unlock(&self->mutex);

                /* Go to previous track. */
                if (track_idx > 0) track_idx--;
                else track_idx = track_count - 1;
                _save_stream_positions(self, track_idx, 0.0f);
                first_track = false;
                continue;
            }
            if (self->reset_requested) {
                self->reset_requested = false;
                pthread_mutex_unlock(&self->mutex);

                /* Restart current track from the beginning. */
                _save_stream_positions(self, track_idx, 0.0f);
                first_track = false;
                continue;
            }
            self->current_track = track_idx;
            pthread_mutex_unlock(&self->mutex);

            /* Open current track */
            const char *path = tracks[track_idx];
            int op_err;
            OggOpusFile *of = op_open_file(path, &op_err);
            if (of == NULL) {
                syslog(LOG_ERR, "Cannot open '%s' (error %d)", path, op_err);
                track_idx = (track_idx + 1) % track_count;
                continue;
            }

            int link = op_current_link(of);
            int ch = MIN(PCM_CHANNELS,
                         op_head(of, link)->channel_count);
            bool mono = (ch == 1);

            /* Seek to saved position if this is the first restored track. */
            if (first_track && start_pos > 0.0f) {
                ogg_int64_t off = _pos_to_pcm_offset(of, start_pos);
                if (off > 0 && op_pcm_seek(of, off) == 0)
                    syslog(LOG_INFO, "Seeked to %.2f%%", start_pos * 100.0f);
                first_track = false;
            }

            /* Record track start time for elapsed-time query. */
            gettimeofday(&self->track_start_time, NULL);

            /* Decode loop for this track */
            int ret;
            while ((ret = op_read(of, pcm_buf,
                                  PCM_BUF_SAMPLES / ch, 0)) > 0) {
                int frames = ret;

                /* Check commands. */
                pthread_mutex_lock(&self->mutex);
                bool stop = self->stop_requested;
                bool next = self->next_requested;
                bool prev = self->prev_requested;
                bool reset = self->reset_requested;
                pthread_mutex_unlock(&self->mutex);

                if (stop) {
                    ogg_int64_t cur = op_pcm_tell(of);
                    _save_stream_positions(self, track_idx,
                                           _pcm_offset_to_pos(of, cur));
                    op_free(of);
                    goto done;
                }
                if (next || prev || reset) {
                    ogg_int64_t cur = op_pcm_tell(of);
                    _save_stream_positions(self, track_idx,
                                           _pcm_offset_to_pos(of, cur));
                    op_free(of);
                    of = NULL;
                    break;
                }

                /* Convert mono to stereo if needed. */
                if (mono) _mono_to_stereo(pcm_buf, frames);

                /* Output to ALSA (or dummy-sleep). */
                if (!is_dummy && pcm != NULL) {
                    snd_pcm_sframes_t wr = snd_pcm_writei(pcm, pcm_buf,
                                                          frames);
                    if (wr < 0) {
                        wr = snd_pcm_recover(pcm, wr, 0);
                        if (wr < 0) {
                            syslog(LOG_ERR, "ALSA write error: %s",
                                   snd_strerror(wr));
                            op_free(of);
                            goto done;
                        }
                    }
                } else if (is_dummy) {
                    struct timespec ts = {
                        .tv_sec = 0,
                        .tv_nsec = (frames * 1000000L) / 48
                    };
                    nanosleep(&ts, NULL);
                }


            }

            /* End of track: save position, advance, loop. */
            if (of != NULL) {
                _save_stream_positions(self, track_idx, 0.0f);
                op_free(of);
                of = NULL;
                track_idx = (track_idx + 1) % track_count;
            }
            first_track = false;
        }

    done:
        if (!is_dummy && pcm != NULL) {
            snd_pcm_drain(pcm);
            snd_pcm_close(pcm);
            pcm = NULL;
        }

    cleanup_tracks:
        self->is_playing = false;
        /* Playlist freed by tn_media_stop / next tn_media_play. */
    }

    if (!is_dummy && pcm != NULL) snd_pcm_close(pcm);
    self->is_playing = false;
    return NULL;
}

// Utility functions

char *find_playlist_filename(char *media_root, uint8_t *card_id) {
    char *dir_path = NULL, *result = NULL;

    size_t plen = snprintf(NULL, 0, "%s/%02X%02X%02X%02X",
                           media_root, card_id[0], card_id[1],
                           card_id[2], card_id[3]);
    dir_path = malloc(plen + 1);
    snprintf(dir_path, plen + 1, "%s/%02X%02X%02X%02X",
             media_root, card_id[0], card_id[1], card_id[2], card_id[3]);

    DIR *dir = opendir(dir_path);
    P_CHECK(dir, goto out);

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        size_t dl = strlen(ent->d_name);
        if (dl > PLAYLIST_SUFFIX_LEN &&
            strcmp(ent->d_name + dl - PLAYLIST_SUFFIX_LEN,
                   PLAYLIST_SUFFIX) == 0) {
            size_t rl = snprintf(NULL, 0, "%s/%s", dir_path, ent->d_name);
            result = malloc(rl + 1);
            snprintf(result, rl + 1, "%s/%s", dir_path, ent->d_name);
            break;
        }
    }
    closedir(dir);

out:
    free(dir_path);
    return result;
}

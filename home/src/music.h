// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_MUSIC_H
#define HM_MUSIC_H

/*
 * Music in Pocketwatch Home (HOM-REQ-018, docs/research/m7-xmb-inventory.md): the MP3 files of
 * ms0:/MUSIC and its folders, played by the firmware's MP3 decoder (sceMp3, on the Media Engine)
 * on a thread of its own, so the music goes on while Home is browsed. The next track follows.
 */

#define HM_TRACKS_MAX 400

typedef struct {
    char path[192];
    char title[96];  /* from the file name: "Title - Artist.mp3" */
    char artist[64]; /* or "" */
} hm_track;

int hm_music_scan(void); /* returns how many tracks */
int hm_music_count(void);
const hm_track *hm_music_track(int i);

void hm_music_play(int i);  /* start track i (the one playing stops) */
void hm_music_toggle(void); /* pause or go on */
void hm_music_stop(void);
void hm_music_skip(int dir); /* the next (1) or previous (-1) track */

int hm_music_current(void); /* the track playing or paused, or -1 */
int hm_music_paused(void);
/* Seconds played and the track's length (0 when not known yet). */
void hm_music_time(unsigned int *played, unsigned int *length);

/* Portable: "Title - Artist.mp3" into its parts (tests/test_music.c). */
void hm_music_name(const char *file, char *title, int tcap, char *artist, int acap);
/* The size of an ID3v2 tag at the start of a file (its first 10 bytes), or 0. */
unsigned int hm_music_id3_size(const unsigned char *head);

#endif

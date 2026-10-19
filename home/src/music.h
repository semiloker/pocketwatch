// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_MUSIC_H
#define HM_MUSIC_H

/*
 * Music in Pocketwatch Home (HOM-REQ-018, docs/research/m7-xmb-inventory.md): the MP3 files of
 * ms0:/MUSIC and its folders, played by the firmware's MP3 decoder (sceMp3, on the Media Engine)
 * on a thread of its own, so the music goes on while Home is browsed. Their ID3 tags give the
 * title, artist and album (read by a thread after the list is made; the file name until then),
 * and the cover (HOM-REQ-027, ADR-036). Seeking re-opens the decoder at the frame nearest to the
 * time asked for; the next track follows by the repeat and shuffle settings.
 */

#define HM_TRACKS_MAX 400

typedef struct {
    char path[192];
    char title[96];  /* the tag's, or from the file name: "Title - Artist.mp3" */
    char artist[64]; /* or "" */
    char album[64];  /* or "" */
    volatile int tagged;
    unsigned int pic_off, pic_len; /* the cover's bytes in the file (the tag's APIC), 0: none */
} hm_track;

int hm_music_scan(void); /* returns how many tracks */
int hm_music_count(void);
const hm_track *hm_music_track(int i);

void hm_music_play(int i);  /* start track i (the one playing stops) */
void hm_music_toggle(void); /* pause or go on */
void hm_music_stop(void);
void hm_music_skip(int dir);     /* the next (1) or previous (-1) track (by shuffle); back: from the
                                    start again when more than 3 s played */
void hm_music_seek(int seconds); /* forward (> 0) or back in the track playing */
void hm_music_mode(int shuffle, int repeat); /* hcfg.h HM_REPEAT_* */
/* The tags thread keeps off the card (USB) or goes on. */
void hm_music_pause_tags(int on);

int hm_music_current(void); /* the track playing or paused, or -1 */
int hm_music_paused(void);
/* Seconds played and the track's length (0 when not known yet). */
void hm_music_time(unsigned int *played, unsigned int *length);

/* Portable: "Title - Artist.mp3" into its parts (tests/test_music.c). */
void hm_music_name(const char *file, char *title, int tcap, char *artist, int acap);
/* The size of an ID3v2 tag at the start of a file (its first 10 bytes), or 0. */
unsigned int hm_music_id3_size(const unsigned char *head);

/* An ID3v2 tag's words and where its picture is (portable). */
typedef struct {
    char title[96], artist[64], album[64]; /* UTF-8 */
    unsigned int pic_off, pic_len;         /* from the file's start; 0: no picture */
    unsigned int tag_len;
} hm_id3;
typedef int (*hm_id3_read_fn)(void *ctx, unsigned int off, void *buf, unsigned int n);
/* Reads the tag at the file's start through read (a few small reads): 0, or < 0 (no tag). */
int hm_id3_read(hm_id3_read_fn read, void *ctx, hm_id3 *out);
/* A text frame's body (its encoding byte first) as UTF-8. */
void hm_id3_text(const unsigned char *s, unsigned int n, char *out, int cap);

/* An MP3 frame's length in bytes from its 4-byte header (Layer III), or < 0. */
int hm_mp3_frame_len(unsigned int header);
/* The first frame in buf like ref (the file's first header: the same version, layer and rate),
   followed by another one: its offset, or < 0. */
int hm_mp3_find_frame(const unsigned char *buf, int n, unsigned int ref);

#endif

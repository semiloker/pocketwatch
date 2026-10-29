// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_SOUND_H
#define HM_SOUND_H

/*
 * Pocketwatch Home's key sounds (HOM-REQ-019): short tones of our own, made once at start
 * (sound_synth.c: no file, nothing of Sony's), played on a thread of their own through one
 * normal audio channel (44.1 kHz mono) beside the music's SRC channel. A new sound cuts the
 * one playing short within a chunk (256 samples, 5.8 ms).
 */

enum { HM_SND_MOVE, HM_SND_TAB, HM_SND_OPEN, HM_SND_BACK, HM_SND_START, HM_SND_ERROR, HM_SNDS };

#define HM_SND_RATE  44100
#define HM_SND_CHUNK 256
#define HM_SND_MAX   (28 * HM_SND_CHUNK) /* samples: the longest, 162 ms */

/* Portable: make sound s into out (16-bit mono, cap samples). Returns its length in samples, a
   multiple of HM_SND_CHUNK, or -1 (no such sound, cap too small). */
int hm_sound_make(int s, short *out, int cap);

void hm_sound_init(void); /* make them, take a channel, start the thread */
void hm_sound_end(void);  /* the thread and the channel go (before a game starts) */
void hm_sound_enable(int on);
void hm_sound_play(int s);

#endif

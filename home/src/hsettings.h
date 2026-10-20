// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_HSETTINGS_H
#define HM_HSETTINGS_H

/*
 * Home's Settings section (HOM-REQ-012, docs/research/m7-xmb-inventory.md): Pocketwatch's own
 * settings (config.txt, language.txt - the same files the XMB's page writes), the system's
 * settings the firmware lets an application read and write (sceUtility system parameters), and
 * the screen and sound ones the HOME screen itself changes (impose, through KUBridge). Every
 * write is read back: a setting the firmware refuses shows its real value again.
 */

enum { HS_HEADER, HS_CHOICE, HS_ACTION };
enum { HS_ACT_NONE, HS_ACT_NICKNAME, HS_ACT_NET_TIME };

/* The categories, each a page of its own (ADR-035), in the order of Home's Settings: the XMB's
   Settings items stand for them in Classic. */
enum {
    HS_CAT_LOOK,        /* style, theme, accent, background, sky, wave, battery percent, motion */
    HS_CAT_POCKETWATCH, /* start screen, sorting, fast save and load, the hub's clock */
    HS_CAT_MEDIA,       /* repeat, shuffle, slideshow */
    HS_CAT_SOUND,       /* key sounds, brightness, AVLS */
    HS_CAT_TIME,        /* date and time formats, zone, summer time */
    HS_CAT_SYSTEM,      /* languages, nickname */
    HS_CAT_NETWORK,     /* WLAN power save */
    HS_CATS
};

/* What a change asks of the caller: its strings again (the language), the look again (the
   background, the colours, the style), the music player's mode again. */
#define HS_TEXTS_CHANGED     1
#define HS_LOOK_CHANGED      2
#define HS_WALLPAPER_CHANGED HS_LOOK_CHANGED
#define HS_MUSIC_CHANGED     3
#define HS_KEYS_CHANGED      4 /* the Enter button: Enter and Back swap */
#define HS_NEXT_GAME         5 /* the game overlay: from the next game on (a toast says so) */

const char *hs_cat_label(int c);
int hs_cat_glyph(int c);                      /* hm_icons.h HM_ICON_* (Modern, Minimal) */
int hs_cat_art(int c);                        /* HM_ART_* + 1 (Classic: the system's icon) */
int hs_cat_rows(int c, int *idx, int max);    /* the rows of a category (indices for hs_*) */
void hs_cat_value(int c, char *out, int cap); /* a word for its row ("Modern"), or "" */

/* The sleep timer (Music and Photos): when it ends (hm_hw_now_ms, 0: none); done once acted on. */
unsigned int hs_timer_end(void);
void hs_timer_done(void);

/* A colour to show with option o of row i (an accent, a sky), 0: none. */
unsigned int hs_swatch(int i, int o);
/* The twelve months' sky colours (for the swatches of Sky Colour). */
void hs_set_skies(const unsigned int colours[12]);

int hs_count(void);
int hs_kind(int i);
int hs_action(int i);                     /* HS_ACTION rows */
const char *hs_label(int i);              /* in the current language */
void hs_value(int i, char *out, int cap); /* the current value's text ("" for headers) */

/* Left or right on a choice row (dir -1 or 1). Returns HS_TEXTS_CHANGED, 0, or < 0 when the
   firmware refused the change. */
int hs_step(int i, int dir);

/* Home's key sounds on (1) or off, as Settings has them (sounds.txt). */
int hs_sounds(void);

/* Home's wallpaper (512 x 272 pixels as Home draws them): read it (0, or < 0 without one), or
   keep a new one made from the picture at path. */
int hs_wallpaper_load(unsigned int *out);
int hs_wallpaper_set(const char *path, const unsigned int *bg);

/* The nickname (UTF-8) as the system has it, and a new one (HS_ACT_NICKNAME). */
void hs_nickname(char *out, int cap);
int hs_set_nickname(const char *utf8);

/* Set Time from Internet's row (HOM-REQ-032): what main.c's thread last did, and when (the time
   it set, "14:05"). */
enum { HS_NET_IDLE, HS_NET_ASKING, HS_NET_DONE, HS_NET_NO_WIFI, HS_NET_NO_PROFILE, HS_NET_FAILED };
void hs_set_net_time(int state, const char *when);

/* An on/off row: 1 off, 2 on; 0 for any other row (Modern draws those as switches). */
int hs_toggle(int i);

/* A choice's options, for the XMB's option panel (Classic): how many, each one's text, the
   current one (< 0 unknown), and choosing one (as hs_step returns). */
int hs_options(int i);
void hs_option_text(int i, int o, char *out, int cap);
int hs_current(int i);
int hs_choose(int i, int o);

/* The selectable row nearest to i in direction dir (headers are skipped); i itself if none. */
int hs_next(int i, int dir);

/* The overlay module's settings (perf.txt): the indicator and the boost Home uses too. */
#include "overlay.h"
const pw_perf_cfg *hs_perf_cfg(void);

#endif

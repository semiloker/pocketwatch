// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_LIBRARY_H
#define HM_LIBRARY_H

#include "home_view.h"

/*
 * The games on the card (HOM-REQ-001, -002, -008): every .iso / .cso in ms0:/ISO and its folders,
 * and every EBOOT.PBP in ms0:/PSP/GAME/<folder>/ (homebrew, PS1 games), with the title and ID
 * from its PARAM.SFO, what Pocketwatch knows of it (play time, save states), and - loaded by a
 * thread, nearest to the chosen one first - its ICON0 and, once the choice settles, its PIC1 as
 * a background. PSP games first, then PS1 games, then homebrew, each by title.
 */

enum { HM_KIND_ISO, HM_KIND_PSN, HM_KIND_PS1, HM_KIND_APP };

#define HM_GAMES_MAX 96

typedef struct {
    int kind;       /* HM_KIND_* */
    char path[192]; /* the image, or the EBOOT.PBP */
    char id[16];
    char title[64];
    char meta[96];
    unsigned int play_s;
    unsigned long long last; /* when it was last played (play-time file), 0: never */
    int states;
    unsigned int *volatile icon; /* HM_ICON_W x HM_ICON_H, set by the loader */
    volatile int icon_tried;
} hm_game;

int hm_library_scan(void); /* returns how many games */
/* The order: by name (0) or the last played first (1); again now (the loader paused). */
void hm_library_sort(int recent);
int hm_library_count(void);
int hm_library_has(const char *image, const char *inner); /* the image has that file */
hm_game *hm_library_game(int i);

/* Start the loader thread; tell it which game is chosen. */
void hm_library_start(void);
void hm_library_focus(int sel);

/* The chosen game's background (HM_W x HM_H, stride 512) once loaded for that game, or NULL. */
const unsigned int *hm_library_background(int sel);

/* USB (HOM-REQ-011): stop the loader's card reads (returns once it is idle) or let it go on. */
void hm_library_pause(int on);
/* After USB: the card may have changed - list it again (loader paused). Returns the count. */
int hm_library_rescan(void);

/* Photos (HOM-REQ-016): ms0:/PICTURE and ms0:/DCIM, two folder levels, newest first. */
#define HM_PHOTOS_MAX 300

typedef struct {
    char path[192];
    char name[64];
    char meta[48];                /* its folder (a game's ID for screenshots) and date */
    unsigned int *volatile thumb; /* HM_ICON_W x HM_ICON_H, kept in shape */
    volatile int tried;
} hm_photo;

int hm_library_photo_count(void);
hm_photo *hm_library_photo(int i);

/* What the loader works for: the games (0), the photos (1), a photo shown whole (2); and which. */
void hm_library_mode(int mode, int sel);
/* The background the loader made for the chosen photo (blurred, or whole when viewing), or NULL. */
const unsigned int *hm_library_photo_background(int sel, int whole);

/* The card's free space in MB, counted by the loader (-1 until then); count it again. */
int hm_library_free_mb(void);
void hm_library_count_free(void);

#endif

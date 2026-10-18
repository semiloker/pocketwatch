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

enum { HM_KIND_ISO, HM_KIND_PS1, HM_KIND_APP };

#define HM_GAMES_MAX 96

typedef struct {
    int kind;       /* HM_KIND_* */
    char path[192]; /* the image, or the EBOOT.PBP */
    char id[16];
    char title[64];
    char meta[96];
    unsigned int play_s;
    int states;
    unsigned int *volatile icon; /* HM_ICON_W x HM_ICON_H, set by the loader */
    volatile int icon_tried;
} hm_game;

int hm_library_scan(void); /* returns how many games */
int hm_library_count(void);
int hm_library_has(const char *image, const char *inner); /* the image has that file */
hm_game *hm_library_game(int i);

/* Start the loader thread; tell it which game is chosen. */
void hm_library_start(void);
void hm_library_focus(int sel);

/* The chosen game's background (HM_W x HM_H, stride 512) once loaded for that game, or NULL. */
const unsigned int *hm_library_background(int sel);

#endif

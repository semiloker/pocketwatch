// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_GAME_H
#define PW_GAME_H

/*
 * Which game is running (SYS-REQ-011): its ID from the disc header (disc0:/UMD_DATA.BIN), its
 * title and parental level from disc0:/PSP_GAME/PARAM.SFO, and the ISO ARK-4 mounted. Read once at
 * plugin start; used for the state folders' names and for refusing a state of another game.
 */

typedef struct {
    int ok;
    char id[16];     /* "ULUS10277" (no dash) */
    char title[128]; /* "Castlevania: The Dracula X Chronicles" */
    char iso[256];   /* "ms0:/ISO/..." or "" for a UMD */
    unsigned int iso_size;
    unsigned int parental;
} pw_game;

void pw_game_init(void);
const pw_game *pw_game_get(void);

#endif

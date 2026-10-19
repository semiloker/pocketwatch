// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_SAVEDATA_H
#define HM_SAVEDATA_H

/*
 * A game's saved data as Home shows it (HOM-REQ-024, ADR-036): the folders of ms0:/PSP/SAVEDATA
 * that start with the game's disc ID (Pocketwatch's own states, PW..., are not among them), each
 * with what its PARAM.SFO says (SAVEDATA_TITLE, SAVEDATA_DETAIL), its size and time, and its
 * ICON0 and PIC1 for the pictures (loaded later by library.c's loader). In Home's own look,
 * instead of the firmware's Saved Data dialog; deleting one removes its files and folder.
 */

#define HM_SAVES_MAX 48

typedef struct {
    char dir[24];     /* the folder's name (ULUS10277000) */
    char title[64];   /* SAVEDATA_TITLE, else TITLE, else the folder */
    char detail[128]; /* SAVEDATA_DETAIL's first line */
    char game[64];    /* TITLE */
    char when[32];    /* the folder's time, in the system's format */
    char meta[96];    /* "2026/10/18 14:05   1.2 MB" */
    unsigned int size_kb;
    int files;
    unsigned long long stamp; /* newest first */
} hm_save;

/* The saves of the game with this disc ID, newest first. Returns how many (at most max). */
int hm_saves_list(const char *disc_id, hm_save *out, int max);

/* ms0:/PSP/SAVEDATA/<dir>/<file> into out. */
void hm_saves_path(const hm_save *s, const char *file, char *out, int cap);

/* Removes the save's files and its folder: 0, or < 0 (then some may be gone). */
int hm_saves_delete(const hm_save *s);

/* Removes a Pocketwatch state folder (PW<disc ID><slot>): 0 or < 0. */
int hm_saves_delete_state(const char *disc_id, int slot);

/* Portable: the first line of a SAVEDATA_DETAIL (up to a newline), at most cap - 1 bytes. */
void hm_saves_first_line(const char *s, unsigned int n, char *out, int cap);

#endif

// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_PLAYTIME_H
#define PW_PLAYTIME_H

#include "core/platform.h"

/*
 * Play time per game (PLT-REQ-001..006, docs/subsystems/playtime.md): a table of games with the
 * time played, sessions, the longest session and when they were first recorded and last played,
 * kept in two text files written in turn (each with a sequence number and a CRC, so a write cut
 * short leaves the other one). Time before Pocketwatch was there is not known and never shown as
 * if it were. Portable: complications/playtime/ reads the system and the card.
 */

#define PW_PT_GAMES_MAX 100
#define PW_PT_TEXT_MAX  (PW_PT_GAMES_MAX * 128 + 256)

typedef struct {
    char id[16];    /* the disc ID ("ULUS10277") */
    char title[48]; /* as of the last session, UTF-8 */
    unsigned int total_s, sessions, longest_s;
    pw_datetime first, last; /* local time: first recorded, last played (the PSP's clock) */
} pw_pt_game;

typedef struct {
    pw_pt_game g[PW_PT_GAMES_MAX];
    int n;
    unsigned int seq; /* the file's sequence number */
} pw_pt_table;

/* Read a file's text (len bytes). Returns the number of games, or < 0 when the file is not
   a complete play-time file (bad CRC, cut short): the table is then empty. */
int pw_pt_parse(pw_pt_table *t, const char *text, unsigned int len);

/* Write t (with t->seq) as text. Returns the length, or < 0 if cap is too small. */
int pw_pt_format(const pw_pt_table *t, char *out, unsigned int cap);

/* The game's record; create: add it when missing (the least recently played one makes room when
   the table is full). NULL when missing and not created. */
pw_pt_game *pw_pt_find(pw_pt_table *t, const char *id, int create);

/* Indices of the games by time played, most first. Returns how many. */
int pw_pt_order(const pw_pt_table *t, int *order, int max);

/* What a game's record held when this boot began (the session adds to it, never twice). */
typedef struct {
    unsigned int total_s, sessions, longest_s;
    int known; /* the game was in the table */
    pw_datetime first;
} pw_pt_base;

void pw_pt_base_of(const pw_pt_game *g, pw_pt_base *b); /* g NULL: a game not seen before */

/* The record after this boot's session of session_s seconds that began at start. */
void pw_pt_apply(pw_pt_game *g, const pw_pt_base *base, unsigned int session_s,
                 const pw_datetime *start, const pw_datetime *now, const char *title);

#endif

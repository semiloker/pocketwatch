// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_PAGES_H
#define HM_PAGES_H

#include "home_view.h"

/*
 * Pocketwatch Home's pages (ADR-035): what each page of each section holds - its rows or tiles,
 * its name, what Enter on a row does - built from the library, the music, the settings and the
 * system. main.c keeps the stack (which page at which depth) and the keys; pages.c fills
 * hm_view's pages from it, at a change and every second.
 */

enum {
    P_GAMES,       /* Games: the games */
    P_GAME,        /* a game's page (Triangle): Play, Save States, Saved Data, Information */
    P_STATES,      /* its save states (pictures; Enter continues from one) */
    P_SAVES,       /* its saved data */
    P_SAVE,        /* one save: what it is, Delete */
    P_INFO,        /* a game's information */
    P_MUSIC,       /* Music: what plays, the tracks */
    P_NOW,         /* what plays, whole */
    P_ALBUMS,      /* Photos: the albums */
    P_ALBUM,       /* one album's pictures */
    P_PLAYTIME,    /* Play Time */
    P_SETTINGS,    /* Settings: the categories */
    P_SET_CAT,     /* one category's settings */
    P_SYSTEM,      /* System: USB, the browser, the pages below, the way out */
    P_SYS_BATTERY, /* the battery's numbers */
    P_SYS_STORAGE, /* the Memory Stick's */
    P_SYS_ABOUT,   /* the PSP's */
    P_SYS_POWER,   /* Sleep, Turn Off, Exit */
    P_COUNT
};

/* What Enter on a row does. */
enum {
    A_NONE,
    A_OPEN,     /* the page arg (P_*), with the row's own argument */
    A_SETTING,  /* a setting (hsettings.c's row arg) */
    A_NICKNAME, /* the keyboard */
    A_USB,
    A_BROWSER,
    A_EXIT, /* to the XMB */
    A_SLEEP,
    A_OFF,
    A_PLAY,   /* start the game */
    A_TRACK,  /* play track arg (or pause the one playing) */
    A_DELETE, /* delete what the page shows (a save) */
};

typedef struct {
    int act, arg, arg2; /* A_*; for A_OPEN: the page and its argument */
} hm_row_act;

/* A page of the stack: what it is and of what (a game, a category, an album, a save). */
typedef struct {
    int id, arg;
} hm_page_id;

/* What the pages show that main.c keeps. */
typedef struct {
    const char *model, *firmware, *cfw, *mac;
    int web_failed;
    int saves_game; /* the game whose saves are listed (-1 none) */
} hm_page_env;

/* Fill page d of v for id (its rows or tiles, name, layout, empty text); positions are kept. */
void hm_page_build(hm_view *v, int d, hm_page_id id, const hm_page_env *env);
/* What Enter on row r of the page id does. */
hm_row_act hm_page_act(hm_page_id id, int r);
/* The root page of a section. */
int hm_page_root(int tab);
/* The text every root needs again after the language changed (the games' lines). */
void hm_pages_texts(void);

/* The lists main.c also uses: a game's states, its saves, an album's pictures. */
#include "savedata.h"
#include "states.h"
int hm_pages_states(int game); /* (re)list the states of game: how many */
hm_state *hm_pages_state(int i);
int hm_pages_saves(int game); /* (re)list the saves of game (their icons by the loader) */
hm_save *hm_pages_save(int i);
int hm_pages_album(int album);   /* the album's pictures: how many */
int hm_pages_album_photo(int k); /* the k-th one's photo index */
int hm_pages_album_at(int k);    /* the album at tile k of the albums page */
void hm_pages_drop(void);        /* everything listed let go (USB) */

#endif

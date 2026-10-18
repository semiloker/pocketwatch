// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_HOME_VIEW_H
#define HM_HOME_VIEW_H

#include "gfx/text.h"
#include "ui/draw.h"

/*
 * Pocketwatch Home's screens in the Modern style (HOM-REQ-001..005, docs/subsystems/home.md): the
 * selected game's own background art behind a row of game icons, its title and what Pocketwatch
 * knows of it; Play Time and System as cards. Portable: everything is passed in; the host renders
 * previews of it.
 */

#define HM_W      480
#define HM_H      272
#define HM_ICON_W 144
#define HM_ICON_H 80

enum { HM_TAB_GAMES, HM_TAB_PLAYTIME, HM_TAB_SYSTEM, HM_TABS };

typedef struct {
    pw_font *title, *body, *big;
    int cross_enter; /* X confirms (outside Japan) */
} hm_ui;

typedef struct {
    const char *title;
    const char *meta;         /* "ULUS10277   12 h 30 min   3 save states", or NULL */
    const unsigned int *icon; /* HM_ICON_W x HM_ICON_H, or NULL (a drawn disc) */
} hm_tile;

typedef struct {
    const char *label, *value;
    int bar; /* 0-256: a bar under the row (Play Time), -1: none */
} hm_row;

typedef struct {
    int tab, tab_pos; /* the tab, and where its marker is (1/256 tabs, animated) */
    const char *tab_names[HM_TABS];
    const char *clock;
    int battery, charging; /* percent or -1 */

    /* Games */
    const hm_tile *tiles;
    int count, sel, selpos; /* the chosen game, and the row's position (1/256, animated) */
    const unsigned int *bg, *bg_prev; /* 512-stride backgrounds (hm_make_*), or NULL */
    int bg_mix;                       /* 0-256: from bg_prev to bg */
    const char *play_label;           /* "Play" */
    const char *empty;                /* shown when there is no game */

    /* Play Time, System: rows on a card */
    const hm_row *rows;
    int nrows, row_sel, row_pos;

    const char *hint_enter, *hint_tabs; /* the bottom hints ("Play", "Section") */
    unsigned int t_ms;
    int appear; /* 0-256: the screen fading in, or out before a game starts */
} hm_view;

void hm_draw(pw_canvas *c, const hm_ui *ui, const hm_view *v);

/* A background from a game's PIC1 (any size): stretched to HM_W x HM_H, blurred and darkened
   towards the bottom, so text stays readable on any art. */
void hm_make_background(const unsigned int *pic, int w, int h, unsigned int *out);
/* The background without art (a slate gradient), into a HM_H x 512 buffer. */
void hm_make_gradient(unsigned int *out);

#endif

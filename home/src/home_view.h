// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_HOME_VIEW_H
#define HM_HOME_VIEW_H

#include "gfx.h"
#include "gfx/image.h"
#include "gfx/text.h"
#include "ui/draw.h"

/*
 * Pocketwatch Home's screens (HOM-REQ-001..021, docs/subsystems/home.md) in the three styles of
 * the in-game menu (HOM-REQ-006, docs/ui/styles.md): Classic - the XMB itself (the month's sky
 * and its wave, the system's category icons, the item column); Modern - section pills, the game
 * row over its art, lists of rows; Minimal - text over the darkened background. Portable:
 * everything is passed in; the host renders previews of it.
 */

#define HM_W      480
#define HM_H      272
#define HM_ICON_W 144
#define HM_ICON_H 80

enum {
    HM_TAB_GAMES,
    HM_TAB_MUSIC,
    HM_TAB_PHOTOS,
    HM_TAB_PLAYTIME,
    HM_TAB_SETTINGS,
    HM_TAB_SYSTEM,
    HM_TABS
};

/* config.txt's style, the same values as the in-game menu's (ui/ui.h PW_STYLE_*) */
enum { HM_STYLE_CLASSIC, HM_STYLE_MODERN, HM_STYLE_MINIMAL, HM_STYLES };

#define HM_PANEL_MAX  8
#define HM_POPUP_MAX  48
#define HM_PHOTO_COLS 4 /* Modern's grid of photos: up / down move by a row */

/* A row's kind: a reading, a choice (left / right change it; Classic: Enter opens its options),
   an action (Enter), the name of a group of rows, a remark (neither of the last two can be
   chosen). */
enum { HM_ROW_INFO, HM_ROW_CHOICE, HM_ROW_ACTION, HM_ROW_HEADER, HM_ROW_NOTE };

/* The system's own pictures (flash0, read at start; never shipped, ADR-013): px NULL when the
   PSP did not have one - then Home draws its own. */
typedef struct {
    pw_image cat[HM_TABS]; /* the XMB's category icons for the sections */
    pw_image btn[4];       /* ui/icons.h PW_BTN_* */
    pw_image battery;      /* four frames one under the other, full to empty */
} hm_art;

typedef struct {
    pw_font *title, *body, *big;
    int cross_enter;   /* X confirms (outside Japan) */
    const hm_art *art; /* or NULL */
} hm_ui;

typedef struct {
    const char *title;
    const char *meta;         /* "ULUS10277   12 h 30 min   3 save states", or NULL */
    const unsigned int *icon; /* HM_ICON_W x HM_ICON_H, or NULL (a drawn disc) */
} hm_tile;

typedef struct {
    const char *label, *value;
    int bar;  /* 0-256: a bar under the row (Play Time, the track playing), -1: none */
    int kind; /* HM_ROW_* (0: a reading) */
    const unsigned int *icon; /* HM_ICON_W x HM_ICON_H (a game's ICON0), or NULL */
    int toggle;               /* an on/off setting: 1 off, 2 on; 0 not one */
} hm_row;

typedef struct {
    int style;        /* HM_STYLE_* */
    int tab, tab_pos; /* the tab, and where its marker is (1/256 tabs, animated) */
    const char *tab_names[HM_TABS];
    const char *clock;
    int battery, charging; /* percent or -1 */

    /* Games (and Photos: the same tiles) */
    const hm_tile *tiles;
    int count, sel, selpos; /* the chosen game, and the row's position (1/256, animated) */
    const unsigned int *bg, *bg_prev; /* 512-stride backgrounds (hm_make_*), or NULL */
    int bg_mix;                       /* 0-256: from bg_prev to bg */
    int wave;                         /* 0-256: the XMB's wave over the background (Classic) */
    const char *play_label;           /* "Play" */
    const char *empty;                /* shown when there is no game */

    /* Music, Play Time, Settings, System: rows */
    const hm_row *rows;
    int nrows, row_sel, row_pos;

    /* Music: what plays (row 0 of its rows: play / pause; the tracks follow) */
    const char *mu_title, *mu_artist, *mu_time; /* NULL: nothing chosen yet */
    int mu_bar;                                 /* 0-256 of the track, -1 unknown */
    int mu_state;                               /* 0 stopped, 1 playing, 2 paused */
    int mu_cur;                                 /* the track playing (its row is mu_cur + 1), -1 */

    const char *hint_enter, *hint_tabs; /* the bottom hints ("Play", "Section") */
    const char *hint_options;           /* Triangle: a game's options, or NULL */
    const char *hint_back;              /* Back, or NULL */
    unsigned int t_ms;
    int appear; /* 0-256: the screen fading in, or out before a game starts */

    /* The HOME panel (HOM-REQ-010): over everything while panel > 0 (0-256, fading) */
    int panel, panel_sel, panel_pos;
    const char *panel_title;
    const char *panel_items[HM_PANEL_MAX];
    const char *panel_values[HM_PANEL_MAX];        /* right-aligned, or NULL */
    const unsigned int *panel_icons[HM_PANEL_MAX]; /* HM_ICON_W x HM_ICON_H pictures, or NULL */
    int panel_count;
    const char *panel_hint; /* bottom right, or NULL */

    /* Classic: a setting's options, the XMB's panel on the right (popup > 0, 0-256) */
    int popup, popup_sel, popup_pos, popup_count;
    const char *popup_items[HM_POPUP_MAX];

    /* A photo whole (HOM-REQ-016): the background alone, a caption that fades (0-256) */
    int viewer, caption;                                              /* caption: 0-256 */
    const char *viewer_caption, *viewer_slideshow, *viewer_wallpaper; /* Triangle, Square */

    /* A message card over everything while sheet > 0 (USB, HOM-REQ-011) */
    int sheet, sheet_busy; /* busy: three dots pulse (t_ms) */
    const char *sheet_title, *sheet_text, *sheet_hint;
    const char *sheet_again; /* Enter: try again (USB), or NULL */

    /* For hm_prefetch: every section's rows (by tab), drawn when their section opens */
    const hm_row *other_rows[HM_TABS];
    int other_nrows[HM_TABS];
} hm_view;

void hm_draw(hm_gfx *g, const hm_ui *ui, const hm_view *v);

/* Prepare what the next frames may draw (ADR-028): the neighbours' titles, the other sections'
   rows, the panel - within the backend's budget, while the GE draws this frame. */
void hm_prefetch(hm_gfx *g, const hm_ui *ui, const hm_view *v);

/* A background from a game's PIC1 (any size): stretched to HM_W x HM_H, blurred and darkened
   towards the bottom, so text stays readable on any art (Modern, Minimal). */
void hm_make_background(const unsigned int *pic, int w, int h, unsigned int *out);
/* The same as the XMB shows a PIC1 (Classic): stretched, sharp, a little darker. */
void hm_make_pic1(const unsigned int *pic, int w, int h, unsigned int *out);
/* The background without art (a slate gradient), into a HM_H x 512 buffer. */
void hm_make_gradient(unsigned int *out);
/* The XMB's sky (a 60 x 34 month picture, or NULL for its plain blue-grey) stretched over it. */
void hm_make_sky(const pw_image *sky, unsigned int *out);
/* A photo as it is: fitted whole into the screen, centred on black (the viewer). */
void hm_make_photo(const unsigned int *pic, int w, int h, unsigned int *out);

/* The row of a list nearest to i in direction dir that can be chosen (not a group's name or a
   remark); i itself if none. */
int hm_row_next(const hm_row *rows, int n, int i, int dir);

#endif

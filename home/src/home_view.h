// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_HOME_VIEW_H
#define HM_HOME_VIEW_H

#include "gfx.h"
#include "gfx/image.h"
#include "keyboard.h"
#include "gfx/text.h"
#include "ui/draw.h"

/*
 * Pocketwatch Home's screens (HOM-REQ-001..021, docs/subsystems/home.md) in the three styles of
 * the in-game menu (HOM-REQ-006, docs/ui/styles.md): Classic - the XMB itself (the month's sky
 * and its wave, the system's category and item icons, the item column, the option list);
 * Modern - section pills, the game row over its art, lists of rows; Minimal - text over the
 * darkened background. Every section is a stack of pages (ADR-035): its first page, then the
 * pages opened from it (a category of settings, a game's page, its saved data, an album, what
 * plays), each sliding in from the right and out again. Portable: everything is passed in; the
 * host renders previews of it.
 */

#define HM_W      480
#define HM_H      272
#define HM_ICON_W 144
#define HM_ICON_H 80
#define HM_COVER  128 /* a track's cover: HM_COVER x HM_COVER */

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
#define HM_ALBUM_COLS 4 /* Modern's grid of albums (two rows on screen) */
#define HM_DEPTH      4 /* pages in a section: its first and three opened from it */

/* A row's kind: a reading, a choice (left / right change it; Classic: Enter opens its options),
   an action (Enter), the name of a group of rows, a remark (neither of the last two can be
   chosen), a way into a page of its own (Enter; an arrow). */
enum { HM_ROW_INFO, HM_ROW_CHOICE, HM_ROW_ACTION, HM_ROW_HEADER, HM_ROW_NOTE, HM_ROW_LINK };

/* What a page shows (each style draws each of them its own way). */
enum {
    HM_PAGE_ROWS,  /* rows (Classic: the items of a section's first page, deeper the option list) */
    HM_PAGE_GAMES, /* tiles: games (Modern: the row of icons over the art) */
    HM_PAGE_ALBUMS, /* tiles: photo albums (Modern: a grid of cards) */
    HM_PAGE_PHOTOS, /* tiles: pictures (Modern: a grid) */
    HM_PAGE_SAVES,  /* tiles: saved data, save states (Modern: a list with the pictures) */
    HM_PAGE_MUSIC,  /* rows: what plays, then the tracks */
    HM_PAGE_NOW,    /* what plays, whole: the cover, the time, the controls */
    HM_PAGE_CAL,    /* the battery's calibration: a screen of its own (hm_calview), any style */
};

/* The battery's calibration (BAT-REQ-011): its steps, what to do now, the numbers, the run's
   charge over time - drawn the same in every style, over the whole screen. */
typedef struct {
    int step;                  /* 1 charge, 2 unplug, 3 run down, 4 measured; 0 not started */
    const char *steps[4];      /* their names */
    const char *title, *what;  /* the page's name; what to do now */
    const char *big, *big_sub; /* the charge ("87%"), how it is powered */
    const char *label[5], *value[5];
    int nstats;
    const unsigned char *pct; /* the chart: the charge (0-100) over the run, oldest first */
    int npct;
    const char *chart_left, *chart_right; /* under it: its start, its length */
    const char *chart_empty;              /* in it while there is nothing */
} hm_calview;

/* The system's own item icons in Classic (topmenu_icon.rco, the XMB's Settings items), by what
   they stand for; hm_art.item[]. */
enum {
    HM_ART_THEME,   /* BY: wrench and screen */
    HM_ART_PHOTO,   /* BW: wrench and camera */
    HM_ART_SYSTEM,  /* BX: wrench and PSP */
    HM_ART_CLOCK,   /* BZ: wrench and clock */
    HM_ART_POWER,   /* CA: wrench and plug */
    HM_ART_SOUND,   /* CB: wrench and speaker */
    HM_ART_NETWORK, /* CD: wrench and globe */
    HM_ART_USB,     /* BU */
    HM_ART_WWW,     /* EU: the browser */
    HM_ART_UMD,     /* BH: a disc */
    HM_ART_MS,      /* BI: the Memory Stick */
    HM_ART_FOLDER,  /* BJ: saved data */
    HM_ART_UPDATE,  /* BT */
    HM_ART_ITEMS
};

/* The system's own pictures (flash0, read at start; never shipped, ADR-013): px NULL when the
   PSP did not have one - then Home draws its own. */
typedef struct {
    pw_image cat[HM_TABS];       /* the XMB's category icons for the sections */
    pw_image btn[4];             /* ui/icons.h PW_BTN_* */
    pw_image battery;            /* four frames one under the other, full to empty */
    pw_image item[HM_ART_ITEMS]; /* HM_ART_* */
} hm_art;

typedef struct {
    pw_font *title, *body, *big;
    int cross_enter;             /* X confirms (outside Japan) */
    const hm_art *art;           /* or NULL */
    unsigned int accent;         /* the theme's colour (switches, bars, the chosen tab): PW_RGB */
    int theme;                   /* HM_THEME_*: Modern's and Minimal's surfaces */
    const unsigned int *glow[3]; /* the aurora's soft lights (HM_GLOW x HM_GLOW), or NULL */
} hm_ui;

#define HM_GLOW 64

/* Home's themes (hcfg.h): how Modern and Minimal paint their cards and text. */
enum { HM_THEME_DARK, HM_THEME_GLASS, HM_THEME_OLED, HM_THEME_PAPER, HM_THEMES };

typedef struct {
    const char *title;
    const char *meta;         /* "ULUS10277   12 h 30 min   3 save states", or NULL */
    const unsigned int *icon; /* HM_ICON_W x HM_ICON_H, or NULL (a drawn disc) */
    int fav;                  /* a favourite: a star on it */
} hm_tile;

typedef struct {
    const char *label, *value;
    int bar;  /* 0-256: a bar under the row (Play Time, the track playing), -1: none */
    int kind; /* HM_ROW_* (0: a reading) */
    const unsigned int *icon; /* HM_ICON_W x HM_ICON_H (a game's ICON0), or NULL */
    int toggle;               /* an on/off setting: 1 off, 2 on; 0 not one */
    int glyph;                /* a drawn icon (hm_icons.h HM_ICON_*, PW_ICON_*), 0: none */
    int art;                  /* Classic: the system's item icon (HM_ART_* + 1), 0: the glyph */
    unsigned int swatch;      /* a colour shown with the value (an accent, a sky), 0: none */
} hm_row;

/* One page of a section. */
typedef struct {
    int layout;               /* HM_PAGE_* */
    int over;                 /* Classic: the XMB's option panel over the page under it */
    const char *title;        /* its name (NULL: a section's first page) */
    const char *sub;          /* a line under the title (a game's ID, play time), or NULL */
    const unsigned int *icon; /* HM_ICON_W x HM_ICON_H by the title (a game's ICON0), or NULL */
    const hm_row *rows;
    int nrows, row_sel, row_pos; /* row_pos: where the cursor is (1/256 rows, animated) */
    const hm_tile *tiles;
    int count, sel, selpos; /* selpos: the row's position (1/256, animated) */
    int grid_pos;           /* a grid's chosen row (1/256 rows, animated): how far it scrolled */
    const char *empty;      /* shown when there is nothing */

    /* Settings two-pane (Modern, ADR-050): the page's rows are the left sidebar (categories);
       opt is the right pane - the highlighted category's options (NULL for Wi-Fi / Calibration,
       which open a page of their own). opt_focus: the right pane has the cursor. */
    const hm_row *opt;
    int opt_n, opt_sel, opt_pos, opt_focus, opt_focus_pos;
    int is_settings; /* the settings root: Modern draws it as two panes */
} hm_page;

typedef struct {
    int style;        /* HM_STYLE_* */
    int tab, tab_pos; /* the tab, and where its marker is (1/256 tabs, animated) */
    const char *tab_names[HM_TABS];
    const char *clock;
    int battery, charging; /* percent or -1 */
    int battery_pct;       /* show the percent beside the battery */

    /* The open section's pages: page[0] its first, page[depth] the one shown; depth_pos follows
       depth (1/256 pages, animated): a page slides in from the right while the one before it
       leaves to the left, and back. Pages above depth stay valid while they slide out. */
    hm_page page[HM_DEPTH];
    int depth, depth_pos;

    const unsigned int *bg, *bg_prev; /* 512-stride backgrounds (hm_make_*), or NULL */
    int bg_mix;                       /* 0-256: from bg_prev to bg */
    int wave;                         /* 0-256: the XMB's wave over the background */
    int dim;                          /* 0-256: darker over a bright wallpaper (Modern, Minimal) */
    int aurora;                       /* 0-256: the aurora's lights drifting over it */
    const char *play_label;           /* "Play" */

    /* Music: what plays (HM_PAGE_MUSIC row 0: play / pause; the tracks follow) */
    const char *mu_title, *mu_artist, *mu_album; /* NULL: nothing chosen yet */
    const char *mu_time, *mu_length;             /* "1:24", "3:51" */
    int mu_bar;                                  /* 0-256 of the track, -1 unknown */
    int mu_state;                                /* 0 stopped, 1 playing, 2 paused */
    int mu_cur;                                  /* the track playing (its row is mu_cur + 1), -1 */
    int mu_shuffle;                              /* 0 / 1 */
    int mu_repeat;                               /* 0 off, 1 all, 2 one */
    int mu_seek;                                 /* -1, 1 while seeking back / on, else 0 */
    const unsigned int *mu_cover;                /* HM_COVER x HM_COVER, or NULL */

    const char *hint_enter, *hint_tabs; /* the bottom hints ("Play", "Section") */
    const char *hint_options;           /* Triangle, or NULL */
    const char *hint_square;            /* Square, or NULL */
    const char *hint_back;              /* Back, or NULL */
    const char *hint_lr, *hint_ud;      /* the d-pad's left / right and up / down, or NULL */
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
    unsigned int popup_swatch[HM_POPUP_MAX]; /* a colour by an option, 0: none */

    /* A photo whole (HOM-REQ-016): the background alone, a caption that fades (0-256) */
    int viewer, caption;
    const char *viewer_caption, *viewer_slideshow, *viewer_wallpaper; /* Triangle, Square */
    const char *viewer_edit;                                          /* Enter: edit, or NULL */

    /* The photo editor (HOM-REQ-026): its tools along the bottom, the chosen one's value */
    int editor;               /* 0-256 while open */
    const char *ed_tools[12]; /* the tools' names */
    const int *ed_glyphs;     /* their icons */
    int ed_count, ed_sel, ed_pos;
    const char *ed_value; /* the chosen tool's value ("+2", "Sepia") */
    const char *ed_hint;  /* what up / down do */

    /* A message card over everything while sheet > 0 (USB, a question) */
    int sheet, sheet_busy; /* busy: dots turn (t_ms) */
    const char *sheet_title, *sheet_text, *sheet_hint;
    const char *sheet_again; /* Enter: try again (USB), confirm (a question), or NULL */
    int sheet_glyph;         /* its icon, 0: the Memory Stick */

    /* A short note at the bottom ("Saved"), fading (0-256) */
    int toast;
    const char *toast_text;
    /* The performance overlay (ADR-043): the frame rate big, then named values; perf_fps NULL:
       off */
    const char *perf_fps; /* the gate: non-NULL while the overlay is on; the FPS number */
    const char *perf_label[6], *perf_value[6]; /* value[0] is the frame time; detailed stats */
    int perf_n;

    /* The volume and brightness indicator (HOM-REQ-034): a pill at the bottom while osd > 0 */
    int osd, osd_glyph, osd_level; /* 0-256 fading; its icon; the bar (0-256) */
    const char *osd_text;          /* "18", "Muted", "3/4  92%" */

    const hm_calview *cal; /* the calibration's screen (a HM_PAGE_CAL page), or NULL */

    /* Home's keyboard over everything while kb_t > 0 (0-256, sliding in), NULL when closed */
    const hm_kb *kb;
    int kb_t;

    /* For hm_prefetch: every section's first rows (by tab), drawn when their section opens */
    const hm_row *other_rows[HM_TABS];
    int other_nrows[HM_TABS];
} hm_view;

/* The page shown. */
static inline const hm_page *hm_top(const hm_view *v)
{
    return &v->page[v->depth];
}

void hm_draw(hm_gfx *g, const hm_ui *ui, const hm_view *v);

/* Prepare what the next frames may draw (ADR-028): the neighbours' titles, the other sections'
   rows, the panel - within the backend's budget, while the GE draws this frame. */
void hm_prefetch(hm_gfx *g, const hm_ui *ui, const hm_view *v);

/* A background from a game's PIC1 (any size): stretched to HM_W x HM_H, blurred and darkened
   towards the bottom, so text stays readable on any art (Modern, Minimal). */
void hm_make_background(const unsigned int *pic, int w, int h, unsigned int *out);
/* The same as the XMB shows a PIC1 (Classic): stretched, sharp, a little darker. */
void hm_make_pic1(const unsigned int *pic, int w, int h, unsigned int *out);
/* The background without art (a slate gradient lit by the accent), into a HM_H x 512 buffer. */
void hm_make_gradient(unsigned int accent, unsigned int *out);
/* A plain dark background lit by the accent from below (the OLED theme: black). */
void hm_make_plain(unsigned int accent, int black, unsigned int *out);
/* A starry night sky with wave ribbons low down (owner 2026-10-23). */
void hm_make_nebula(unsigned int accent, unsigned int *out);
/* One of the aurora's lights: a soft round glow of colour c (HM_GLOW x HM_GLOW, alpha on top). */
void hm_make_glow(unsigned int c, unsigned int *out);
/* The XMB's sky (a 60 x 34 month picture, or NULL for its plain blue-grey) stretched over it. */
void hm_make_sky(const pw_image *sky, unsigned int *out);
/* A photo as it is: fitted whole into the screen, centred on black (the viewer). */
void hm_make_photo(const unsigned int *pic, int w, int h, unsigned int *out);

/* The row of a list nearest to i in direction dir that can be chosen (not a group's name or a
   remark); i itself if none. */
int hm_row_next(const hm_row *rows, int n, int i, int dir);

#endif

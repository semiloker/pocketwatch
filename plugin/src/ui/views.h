// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_VIEWS_H
#define PW_VIEWS_H

#include "gfx/image.h"
#include "ui/ui.h"
#include "ui/volume.h"

/*
 * Drawing of every menu screen, separated from input and the display so the host can render them
 * (tests, previews, golden images). The look is the PSP's own (docs/ui/README.md): the XMB's
 * category icon and item column over the XMB background, the corner clock and battery, the HOME
 * screen's dialogs. Portable.
 */

typedef struct {
    const pw_image *icon, *glow; /* the system's pictures (normal, selected), or NULL */
    int fallback;                /* PW_ICON_* drawn when there is no picture */
    const char *title;
    const char *desc; /* sub-text under the selected item, or NULL */
    int enabled;
} pw_hub_row;

typedef struct {
    const pw_image *icon;
    int fallback;
    const char *label;
    const char *subtitle; /* the game's title (the modern and minimal styles show it), or NULL */
} pw_hub_cat;

/* The corner: clock text, battery percent (-1: none), charging (the battery fills in a loop). */
void pw_view_status(pw_canvas *c, const pw_ui *ui, const char *clock, int battery, int charging,
                    int alpha, unsigned int t_ms);

/* The XMB-style column. selpos: animated cursor position (1/256 rows); fade 0-256. */
void pw_view_hub(pw_canvas *c, const pw_ui *ui, const pw_hub_cat *cat, const pw_hub_row *rows,
                 int count, int selpos, int fade);

/* One slot of the state list (the Saved Data Utility's layout). */
typedef struct {
    int exists;
    const unsigned int *icon; /* 144x80 pixels, or NULL */
    const char *title;        /* "Save State 2" / "New Save State" */
    const char *info;         /* "2026/10/16 14:05   18.2 MB" */
} pw_slot_row;

/* heading: "Save State" / "Load State"; game: the game's title; selpos: the animated cursor
   (1/256 slots); can_delete: show the triangle hint; appear: the opening fade 0-256. */
void pw_view_slots(pw_canvas *c, const pw_ui *ui, const char *heading, const char *game,
                   const pw_slot_row *rows, int count, int selpos, int can_delete, int appear);

enum { PW_VIEW_CONFIRM, PW_VIEW_MESSAGE };

/* The HOME screen's dialog: line2 may be NULL; yes = the selected choice (confirm); appear: its
   fade-in 0-256. */
void pw_view_dialog(pw_canvas *c, const pw_ui *ui, int kind, const char *line1, const char *line2,
                    int yes, int appear);
void pw_view_busy(pw_canvas *c, const pw_ui *ui, const char *text, unsigned int t_ms);

/* The volume bar over any screen while v->alpha > 0 (ui/volume.h): Classic after the PSP's own
   (a speaker and 30 steps on a dark strip at the bottom), Modern a pill on the right edge, Minimal
   a line and the number. */
void pw_view_volume(pw_canvas *c, const pw_ui *ui, const pw_volume *v);

/* Shared pieces. */
/* A speaker in a 16 x 16 px box at (x, y): waves, or a cross when muted. */
void pw_view_speaker(pw_canvas *c, int x, int y, int muted, unsigned int color, int alpha);
void pw_view_hints(pw_canvas *c, const pw_ui *ui, const char *enter_label, const char *back_label,
                   int alpha);
void pw_view_text_centered(pw_canvas *c, pw_font *f, int cx, int y, const char *s,
                           unsigned int color, int alpha);
void pw_view_button(pw_canvas *c, const pw_ui *ui, int which, int cx, int cy, int alpha);

/* Break text at spaces into lines no wider than max (out[n..cap-1]; the last takes the rest). */
#define PW_VIEW_LINE_MAX 160
int pw_view_wrap(pw_font *f, const char *s, int max, char (*out)[PW_VIEW_LINE_MAX], int n, int cap);

#endif

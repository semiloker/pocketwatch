// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_VIEWS_H
#define PW_VIEWS_H

#include "gfx/image.h"
#include "ui/ui.h"

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
} pw_hub_cat;

/* The corner: clock text, battery percent (-1: none), charging. */
void pw_view_status(pw_canvas *c, const pw_ui *ui, const char *clock, int battery, int charging,
                    int alpha);

/* The XMB-style column. selpos: animated cursor position (1/256 rows); fade 0-256. */
void pw_view_hub(pw_canvas *c, const pw_ui *ui, const pw_hub_cat *cat, const pw_hub_row *rows,
                 int count, int selpos, int fade);

enum { PW_VIEW_CONFIRM, PW_VIEW_MESSAGE };

/* The HOME screen's dialog: line2 may be NULL; yes = the selected choice (confirm). */
void pw_view_dialog(pw_canvas *c, const pw_ui *ui, int kind, const char *line1, const char *line2,
                    int yes);
void pw_view_busy(pw_canvas *c, const pw_ui *ui, const char *text, unsigned int t_ms);

/* Shared pieces. */
void pw_view_hints(pw_canvas *c, const pw_ui *ui, const char *enter_label, const char *back_label,
                   int alpha);
void pw_view_text_centered(pw_canvas *c, pw_font *f, int cx, int y, const char *s,
                           unsigned int color, int alpha);

#endif

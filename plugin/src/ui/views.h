// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_VIEWS_H
#define PW_VIEWS_H

#include "ui/ui.h"

/*
 * Drawing of every menu screen, separated from input and the display so the host can render them
 * (tests, previews, golden images). Geometry: docs/ui/README.md and
 * docs/research/m4-native-look.md. Portable.
 */

typedef struct {
    int icon; /* PW_ICON_* */
    const char *title;
    const char *desc;
    int enabled;
} pw_hub_row;

/* The XMB-style item column. selpos: animated cursor position (1/256 rows); fade 0-256. */
void pw_view_hub(pw_canvas *c, const pw_ui *ui, const pw_hub_row *rows, int count, int selpos,
                 int fade);

enum { PW_VIEW_CONFIRM, PW_VIEW_MESSAGE };

/* Message dialog over the background: line2 may be NULL; yes = the selected choice (confirm). */
void pw_view_dialog(pw_canvas *c, const pw_ui *ui, int kind, const char *line1, const char *line2,
                    int yes);
void pw_view_busy(pw_canvas *c, const pw_ui *ui, const char *text);

/* Shared pieces. */
void pw_view_hints(pw_canvas *c, const pw_ui *ui, const char *enter_label, const char *back_label,
                   int alpha);
void pw_view_text_centered(pw_canvas *c, pw_font *f, int cx, int y, const char *s,
                           unsigned int color, int alpha);

#endif

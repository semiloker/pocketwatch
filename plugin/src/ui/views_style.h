// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_VIEWS_STYLE_H
#define PW_VIEWS_STYLE_H

#include "ui/views.h"

/*
 * The alternative styles of every screen (docs/ui/styles.md). Same parameters as the pw_view_*
 * functions in views.h, which call these when ui->style asks for them. Portable.
 */

#define PW_DECLARE_STYLE(name)                                                                     \
    void name##_status(pw_canvas *c, const pw_ui *ui, const char *clock, int battery,              \
                       int charging, int alpha, unsigned int t_ms);                                \
    void name##_hub(pw_canvas *c, const pw_ui *ui, const pw_hub_cat *cat, const pw_hub_row *rows,  \
                    int count, int selpos, int fade);                                              \
    void name##_slots(pw_canvas *c, const pw_ui *ui, const char *heading, const char *game,        \
                      const pw_slot_row *rows, int count, int selpos, int can_delete, int appear); \
    void name##_settings(pw_canvas *c, const pw_ui *ui, const pw_hub_cat *cat,                     \
                         const pw_setting_row *rows, int count, int sel,                           \
                         const char *const *options, int n_options, int opt_sel, int opt_appear);  \
    void name##_dialog(pw_canvas *c, const pw_ui *ui, int kind, const char *line1,                 \
                       const char *line2, int yes, int appear);                                    \
    void name##_busy(pw_canvas *c, const pw_ui *ui, const char *text, unsigned int t_ms);          \
    void name##_hints(pw_canvas *c, const pw_ui *ui, const char *enter_label,                      \
                      const char *back_label, int alpha);

PW_DECLARE_STYLE(pw_modern)
PW_DECLARE_STYLE(pw_minimal)

#endif

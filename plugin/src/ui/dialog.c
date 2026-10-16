// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspctrl.h>

#include "ui/dialog.h"
#include "ui/fb.h"
#include "ui/views.h"

int pw_dialog_confirm(pw_ui *ui, const char *line1, const char *line2, int default_yes)
{
    int yes = default_yes != 0, dirty = 1;

    pw_ui_flush_input(ui);
    for (;;) {
        unsigned int ev;

        if (dirty) {
            pw_view_dialog(pw_fb_begin(256), ui, PW_VIEW_CONFIRM, line1, line2, yes);
            pw_fb_present();
            dirty = 0;
        }
        ev = pw_ui_poll(ui);
        if (ev & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT)) {
            yes = !yes;
            dirty = 1;
        } else if (ev & ui->enter) {
            pw_ui_flush_input(ui);
            return yes;
        } else if (ev & (ui->back | PSP_CTRL_HOME)) {
            pw_ui_flush_input(ui);
            return 0;
        }
    }
}

void pw_dialog_message(pw_ui *ui, const char *line1, const char *line2)
{
    pw_view_dialog(pw_fb_begin(256), ui, PW_VIEW_MESSAGE, line1, line2, 1);
    pw_fb_present();
    pw_ui_flush_input(ui);
    for (;;) {
        unsigned int ev = pw_ui_poll(ui);

        if (ev & (ui->enter | ui->back | PSP_CTRL_HOME)) {
            pw_ui_flush_input(ui);
            return;
        }
    }
}

void pw_dialog_busy(pw_ui *ui, const char *text)
{
    pw_view_busy(pw_fb_begin(256), ui, text);
    pw_fb_present();
}

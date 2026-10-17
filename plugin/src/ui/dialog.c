// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspctrl.h>
#include <psppower.h>

#include "ui/anim.h"
#include "ui/dialog.h"
#include "ui/fb.h"
#include "ui/views.h"

#define APPEAR_MS 150 /* a dialog fades in, like the system's */

/* Every dialog frame: the backdrop (the XMB's moving wave), then the dialog. */
static void frame(pw_ui *ui, int kind, const char *line1, const char *line2, int yes,
                  unsigned int t_open)
{
    unsigned int now = pw_ui_now_ms();

    pw_view_dialog(pw_fb_begin(256, now), ui, kind, line1, line2, yes,
                   pw_ramp(now, t_open, APPEAR_MS));
    pw_fb_present();
}

int pw_dialog_confirm(pw_ui *ui, const char *line1, const char *line2, int default_yes)
{
    int yes = default_yes != 0;
    unsigned int t_open = pw_ui_now_ms();

    pw_ui_flush_input(ui);
    for (;;) {
        unsigned int ev;

        frame(ui, PW_VIEW_CONFIRM, line1, line2, yes, t_open);
        ev = pw_ui_poll(ui);
        if (ev & (PSP_CTRL_LEFT | PSP_CTRL_RIGHT)) {
            yes = !yes;
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
    unsigned int t_open = pw_ui_now_ms();

    pw_ui_flush_input(ui);
    for (;;) {
        unsigned int ev;

        frame(ui, PW_VIEW_MESSAGE, line1, line2, 1, t_open);
        ev = pw_ui_poll(ui);
        if (ev & (ui->enter | ui->back | PSP_CTRL_HOME)) {
            pw_ui_flush_input(ui);
            return;
        }
    }
}

void pw_dialog_busy(pw_ui *ui, const char *text)
{
    unsigned int t = pw_ui_now_ms();

    pw_view_busy(pw_fb_begin(256, t), ui, text, t);
    pw_fb_present();
    if (t - ui->t_tick > 1000) { /* long card writes: keep the backlight on */
        scePowerTick(0);
        ui->t_tick = t;
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspctrl.h>

#include "complications/states/store.h"
#include "core/game.h"
#include "core/mem.h"
#include "core/sysinfo.h"
#include "stb_sprintf.h"
#include "ui/dialog.h"
#include "ui/fb.h"
#include "ui/slots.h"
#include "ui/views.h"
#include "ui/i18n.h"

#define ICON_PIXELS (PW_STORE_ICON_W * PW_STORE_ICON_H)

typedef struct {
    pw_slot_info info[PW_STORE_SLOTS];
    pw_slot_row rows[PW_STORE_SLOTS];
    int map[PW_STORE_SLOTS]; /* row -> slot */
    int count;
    char title[PW_STORE_SLOTS][32];
    char line[PW_STORE_SLOTS][48];
} list;

static void build(list *l, int mode, unsigned int *icons)
{
    pw_store_scan(l->info, icons);
    l->count = 0;
    for (int s = 0; s < PW_STORE_SLOTS; s++) {
        const pw_slot_info *in = &l->info[s];
        pw_slot_row *r = &l->rows[l->count];
        char date[32];

        if (mode == PW_SLOTS_LOAD && !in->exists)
            continue;
        l->map[l->count] = s;
        r->exists = in->exists;
        r->icon = in->has_icon ? in->icon : 0;
        if (in->exists) {
            pw_sysinfo_format_date(&in->saved, date, sizeof(date));
            stbsp_snprintf(l->line[l->count], sizeof(l->line[0]), pw_tr(PW_S_SIZE_MB), date,
                           in->size / (1024 * 1024), in->size % (1024 * 1024) * 10 / (1024 * 1024));
            stbsp_snprintf(l->title[l->count], sizeof(l->title[0]), pw_tr(PW_S_SLOT_TITLE), s + 1);
            r->info = l->line[l->count];
        } else {
            stbsp_snprintf(l->title[l->count], sizeof(l->title[0]), pw_tr(PW_S_NEW_SLOT), s + 1);
            r->info = 0;
        }
        r->title = l->title[l->count];
        l->count++;
    }
}

int pw_slots_pick(pw_ui *ui, int mode, int start)
{
    static list l;
    pw_mem_block mem;
    unsigned int *icons = 0;
    int sel = 0, result = -1;

    if (pw_mem_alloc(&mem, "PwSlotIcons", PW_STORE_SLOTS * ICON_PIXELS * 4) >= 0)
        icons = mem.ptr;
    build(&l, mode, icons);
    if (l.count == 0) {
        pw_dialog_message(ui, pw_tr(PW_S_NO_STATES), 0);
        goto out;
    }
    for (int i = 0; i < l.count; i++)
        if (l.map[i] == start)
            sel = i;
    pw_ui_flush_input(ui);
    for (;;) {
        unsigned int ev;

        pw_view_slots(pw_fb_begin(256, pw_ui_now_ms()), ui,
                      mode == PW_SLOTS_SAVE ? pw_tr(PW_S_SAVE_STATE) : pw_tr(PW_S_LOAD_STATE),
                      pw_game_get()->title, l.rows, l.count, sel, 1);
        pw_fb_present();
        ev = pw_ui_poll(ui);
        if (ev & PSP_CTRL_UP) {
            if (sel > 0)
                sel--;
        } else if (ev & PSP_CTRL_DOWN) {
            if (sel < l.count - 1)
                sel++;
        } else if (ev & (ui->back | PSP_CTRL_HOME)) {
            break;
        } else if (ev & ui->enter) {
            char q[48];

            if (mode == PW_SLOTS_SAVE && l.rows[sel].exists) {
                stbsp_snprintf(q, sizeof(q), pw_tr(PW_S_CONFIRM_OVERWRITE), l.map[sel] + 1);
                if (!pw_dialog_confirm(ui, q, pw_tr(PW_S_CONFIRM_OVERWRITE2), 0))
                    continue;
            }
            result = l.map[sel];
            break;
        } else if ((ev & PSP_CTRL_TRIANGLE) && l.rows[sel].exists) {
            char q[48];

            stbsp_snprintf(q, sizeof(q), pw_tr(PW_S_CONFIRM_DELETE), l.map[sel] + 1);
            if (pw_dialog_confirm(ui, q, 0, 0)) {
                pw_store_delete(l.map[sel]);
                build(&l, mode, icons);
                if (l.count == 0)
                    break;
                if (sel >= l.count)
                    sel = l.count - 1;
            }
        }
    }
out:
    if (icons)
        pw_mem_free(&mem);
    pw_ui_flush_input(ui);
    return result;
}

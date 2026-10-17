// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspctrl.h>

#include "complications/registry.h"
#include "complications/states/engine.h"
#include "core/config.h"
#include "core/game.h"
#include "core/sysinfo.h"
#include "gfx/assets.h"
#include "ui/fb.h"
#include "ui/hub.h"
#include "ui/i18n.h"
#include "ui/icons.h"
#include "ui/ui.h"
#include "ui/views.h"

#define FADE_IN_MS  200
#define FADE_OUT_MS 150
#define STATUS_MS   1000
#define ROWS_MAX    12

/*
 * The hub: the XMB-style column of every complication's rows (complications/table.c). It draws,
 * moves the cursor and hands Enter to the row's complication; what a row does is not its business
 * (CPL-REQ-002).
 */
typedef struct {
    pw_ui ui;
    pw_cpl_env env;
    pw_hub_cat cat;
    pw_hub_entry entries[ROWS_MAX];
    pw_hub_row rows[ROWS_MAX];
    int count, sel, selpos; /* selpos: animated position of the cursor, 1/256 rows */
    char clock[24];
    int battery, charging;
    unsigned int t_status;
    unsigned int resume_seen; /* pw_engine_last_resume_ms when the screen was last captured */
} hub;

static int g_last_sel = 0; /* the hub reopens on the last row used */

static void build(hub *h)
{
    h->cat.icon = pw_assets_art(PW_ART_CAT_GAME);
    h->cat.fallback = PW_ICON_WATCH;
    h->cat.label = pw_tr(PW_S_GAME);
    h->cat.subtitle = pw_game_get()->title[0] ? pw_game_get()->title : 0;
    h->count = pw_registry_collect(pw_complications, pw_complication_count, h->entries, ROWS_MAX);
    for (int i = 0; i < h->count; i++) {
        const pw_cpl_item *it = &h->entries[i].item;
        pw_hub_row *r = &h->rows[i];

        r->icon = it->art >= 0 ? pw_assets_art(it->art) : 0;
        r->glow = it->glow >= 0 ? pw_assets_art(it->glow) : 0;
        r->fallback = it->fallback;
        r->title = it->title;
        r->desc = it->desc[0] ? it->desc : 0;
        r->enabled = it->enabled;
    }
    if (h->sel >= h->count)
        h->sel = h->count > 0 ? h->count - 1 : 0;
}

static void refresh_status(hub *h, int force)
{
    unsigned int now = pw_ui_now_ms();

    if (!force && now - h->t_status < STATUS_MS)
        return;
    h->t_status = now;
    pw_sysinfo_clock(h->clock, sizeof(h->clock));
    h->battery = pw_sysinfo_battery(&h->charging);
    if (pw_registry_changed(pw_complications, pw_complication_count))
        build(h);
}

static void draw(hub *h, int show)
{
    pw_canvas *c = pw_fb_begin(show, pw_ui_now_ms());

    if (pw_config_get()->status)
        pw_view_status(c, &h->ui, h->clock, h->battery, h->charging, show);
    pw_view_hub(c, &h->ui, &h->cat, h->rows, h->count, h->selpos, show);
    pw_fb_present();
}

static void fade(hub *h, int from, int to, int ms)
{
    unsigned int t0 = pw_ui_now_ms();

    for (;;) {
        int t = (int)(pw_ui_now_ms() - t0);

        if (t >= ms)
            break;
        draw(h, from + (to - from) * t / ms);
        pw_ui_poll(&h->ui);
    }
    draw(h, to);
}

int pw_hub_run(const pw_freeze_set *frozen)
{
    static hub h; /* the thread's stack stays small */
    int result = PW_HUB_CLOSE;

    pw_ui_init(&h.ui);
    h.env.ui = &h.ui;
    h.env.frozen = frozen;
    pw_registry_open(pw_complications, pw_complication_count);
    h.sel = g_last_sel;
    build(&h);
    refresh_status(&h, 1);
    h.resume_seen = pw_engine_last_resume_ms();
    h.selpos = h.sel * 256;
    fade(&h, 0, 256, FADE_IN_MS);
    pw_ui_flush_input(&h.ui);

    for (;;) {
        unsigned int ev;
        int target = h.sel * 256;

        if (h.selpos != target) { /* ease towards the cursor, like the XMB's list */
            int d = target - h.selpos;

            h.selpos += d / 3 != 0 ? d / 3 : (d > 0 ? 1 : -1);
            if (d < 8 && d > -8)
                h.selpos = target;
        }
        refresh_status(&h, 0);
        if (pw_engine_last_resume_ms() != h.resume_seen) {
            /* The PSP slept (power switch) or a save/load suspended it: the screen's buffers
               in P11 were overwritten by the firmware's ME save. */
            h.resume_seen = pw_engine_last_resume_ms();
            pw_fb_recapture();
        }
        draw(&h, 256); /* every frame: the wave moves */
        ev = pw_ui_poll(&h.ui);
        if (ev & PSP_CTRL_UP) {
            if (h.sel > 0)
                h.sel--;
        } else if (ev & PSP_CTRL_DOWN) {
            if (h.sel < h.count - 1)
                h.sel++;
        } else if (ev & (h.ui.back | PSP_CTRL_HOME | PSP_CTRL_NOTE)) {
            break;
        } else if ((ev & h.ui.enter) && h.count > 0) {
            const pw_hub_entry *e = &h.entries[h.sel];
            int ret;

            g_last_sel = h.sel;
            ret = e->cpl->run(e->index, &h.env);
            if (ret == PW_CPL_LOADED)
                return PW_HUB_LOADED; /* back in the saved moment; the screen is gone */
            if (ret == PW_CPL_QUIT) {
                result = PW_HUB_QUIT;
                break;
            }
            if (ret == PW_CPL_CLOSE)
                break;
            build(&h);
            refresh_status(&h, 1);
            pw_ui_flush_input(&h.ui);
        }
    }
    g_last_sel = h.sel;
    fade(&h, 256, 0, FADE_OUT_MS);
    return result;
}

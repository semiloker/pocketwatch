// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspctrl.h>
#include <string.h>

#include "complications/registry.h"
#include "complications/states/engine.h"
#include "core/config.h"
#include "core/game.h"
#include "core/log.h"
#include "core/sysinfo.h"
#include "gfx/assets.h"
#include "ui/anim.h"
#include "ui/backdrop.h"
#include "ui/cpuwatch.h"
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
    pw_frame_stats stats;
} hub;

static int g_last_sel = 0; /* the hub reopens on the last row used */

/* The clock and battery corner, kept as drawn (ADR-029): it changes once a minute but cost a
   fifth of a Modern frame (host bench). The pixels of the top right, over a still backdrop, in the
   screen's spare page; redrawn only when what it shows - or the backdrop under it - changes. */
#define CORNER_X 240
#define CORNER_H 40

typedef struct {
    char clock[24];
    int battery, charging, show, style;
    unsigned int backdrop;
} corner_key;

static corner_key g_corner;
static int g_corner_ok;

static void status(hub *h, pw_canvas *c, int show, unsigned int t_ms)
{
    unsigned int bytes, *cache = pw_fb_spare(&bytes);
    int w = c->w - CORNER_X;
    corner_key k;

    memset(&k, 0, sizeof(k));
    strcpy(k.clock, h->clock);
    k.battery = h->battery;
    k.charging = h->charging;
    k.show = show;
    k.style = h->ui.style;
    k.backdrop = pw_fb_backdrop_version();
    if (!cache || bytes < (unsigned int)(w * CORNER_H * 4)) {
        g_corner_ok = 0;
        pw_view_status(c, &h->ui, h->clock, h->battery, h->charging, show, t_ms);
        return;
    }
    if (g_corner_ok && memcmp(&k, &g_corner, sizeof(k)) == 0) {
        for (int y = 0; y < CORNER_H; y++)
            memcpy(c->px + y * c->stride + CORNER_X, cache + y * w, (unsigned int)w * 4);
        return;
    }
    pw_view_status(c, &h->ui, h->clock, h->battery, h->charging, show, t_ms);
    for (int y = 0; y < CORNER_H; y++)
        memcpy(cache + y * w, c->px + y * c->stride + CORNER_X, (unsigned int)w * 4);
    g_corner = k;
    g_corner_ok = 1;
}

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

/* The corner and the rows' state, once a second. Returns 1 when something on screen changed. */
static int refresh_status(hub *h, int force)
{
    unsigned int now = pw_ui_now_ms();
    char clock[sizeof(h->clock)];
    int battery, charging, changed;

    if (!force && now - h->t_status < STATUS_MS)
        return 0;
    h->t_status = now;
    pw_sysinfo_clock(clock, sizeof(clock));
    battery = pw_sysinfo_battery(&charging);
    changed = force || strcmp(clock, h->clock) != 0 || battery != h->battery ||
              charging != h->charging || charging; /* a charging battery animates */
    strcpy(h->clock, clock);
    h->battery = battery;
    h->charging = charging;
    if (pw_registry_changed(pw_complications, pw_complication_count)) {
        build(h);
        changed = 1;
    }
    return changed;
}

static void draw(hub *h, int show)
{
    unsigned int t0 = pw_ui_now_us(), t1, t2, t3;
    pw_canvas *c = pw_fb_begin(show, t0 / 1000);

    t1 = pw_ui_now_us();
    if (pw_config_get()->status)
        status(h, c, show, t0 / 1000);
    pw_view_hub(c, &h->ui, &h->cat, h->rows, h->count, h->selpos, show);
    t2 = pw_ui_now_us();
    pw_fb_present();
    t3 = pw_ui_now_us();
    if (h->stats.frames++ == 0)
        h->stats.t_first = t0 / 1000;
    h->stats.t_last = t3 / 1000;
    h->stats.backdrop += t1 - t0;
    h->stats.content += t2 - t1;
    h->stats.present += t3 - t2;
    if (t3 - t0 > h->stats.worst)
        h->stats.worst = t3 - t0;
}

static void log_stats(const hub *h)
{
    const pw_frame_stats *s = &h->stats;
    unsigned int span = s->t_last - s->t_first;

    if (s->frames < 2 || span == 0)
        return;
    pw_log("hub: style %d, %u ms: %u frames drawn in %u vblank waits; a frame: backdrop %u us, "
           "content %u us, present %u us, worst %u us; waiting %u us a loop",
           h->ui.style, span, s->frames, s->loops, s->backdrop / s->frames, s->content / s->frames,
           s->present / s->frames, s->worst, s->loops ? s->poll / s->loops : 0);
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
    memset(&h.stats, 0, sizeof(h.stats));
    pw_cpuwatch_start();
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

    for (unsigned int t_prev = pw_ui_now_ms(), dirty = 1;;) {
        unsigned int ev, now = pw_ui_now_ms(), t_poll;
        int was = h.selpos;

        /* ease towards the cursor like the XMB's list: by time, whatever the frame rate */
        h.selpos = pw_ease(h.selpos, h.sel * 256, (int)(now - t_prev), PW_ANIM_TAU_MS, 6);
        t_prev = now;
        if (refresh_status(&h, 0))
            dirty = 1;
        if (pw_engine_last_resume_ms() != h.resume_seen) {
            /* The PSP slept (power switch) or a save/load suspended it: the screen's buffers
               in P11 were overwritten by the firmware's ME save. */
            h.resume_seen = pw_engine_last_resume_ms();
            pw_fb_recapture();
            dirty = 1;
        }
        /* Draw only what changed: a still Modern or Minimal screen costs nothing; the XMB's
           wave moves, so Classic draws every frame. */
        if (dirty || h.selpos != was || pw_backdrop_moves(h.ui.style)) {
            draw(&h, 256);
            dirty = 0;
        }
        t_poll = pw_ui_now_us();
        ev = pw_ui_poll(&h.ui);
        h.stats.poll += pw_ui_now_us() - t_poll;
        h.stats.loops++;
        if (ev)
            dirty = 1;
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
            if (ret == PW_CPL_LOADED) {
                log_stats(&h);
                pw_cpuwatch_stop("hub");
                return PW_HUB_LOADED; /* back in the saved moment; the screen is gone */
            }
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
    log_stats(&h);
    pw_cpuwatch_stop("hub");
    return result;
}

int pw_hub_startup(const pw_freeze_set *frozen, const pw_complication *cpl)
{
    static pw_ui ui;
    pw_cpl_env env;
    int ret;

    pw_ui_init(&ui);
    env.ui = &ui;
    env.frozen = frozen;
    ret = cpl->startup(&env);
    return ret == PW_CPL_LOADED ? PW_HUB_LOADED : ret == PW_CPL_QUIT ? PW_HUB_QUIT : PW_HUB_CLOSE;
}

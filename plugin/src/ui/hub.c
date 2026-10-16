// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspctrl.h>
#include <psprtc.h>

#include "complications/states/engine.h"
#include "core/log.h"
#include "core/sysinfo.h"
#include "gfx/assets.h"
#include "stb_sprintf.h"
#include "ui/dialog.h"
#include "ui/fb.h"
#include "ui/hub.h"
#include "ui/icons.h"
#include "ui/ui.h"
#include "ui/views.h"
#ifdef PW_DEBUG
#include "debug/cardtest.h"
#endif

#define FADE_IN_MS  200
#define FADE_OUT_MS 150
#define DESC_MAX    80
#define STATUS_MS   1000

enum { IT_RESUME, IT_SAVE, IT_LOAD, IT_QUIT, IT_CARD, IT_COUNT };

typedef struct {
    int id;
    pw_hub_row row;
    char desc[DESC_MAX];
} item;

typedef struct {
    pw_ui ui;
    const pw_freeze_set *frozen;
    pw_hub_cat cat;
    item items[IT_COUNT];
    int count, sel, selpos; /* selpos: animated position of the cursor, 1/256 rows */
    char saved_at[16];      /* "14:05" of the state held, or "" */
    char clock[24];
    int battery, charging;
    unsigned int t_status;
} hub;

static int g_last_sel = 0; /* the hub reopens on the last item used */

static void clock_hm(char *out, unsigned int cap)
{
    ScePspDateTime t;

    if (sceRtcGetCurrentClockLocalTime(&t) < 0)
        stbsp_snprintf(out, (int)cap, "--:--");
    else
        stbsp_snprintf(out, (int)cap, "%d:%02d", t.hour, t.minute);
}

static void add(hub *h, int id, int art, int glow, int fallback, const char *title, int enabled,
                const char *desc)
{
    item *it = &h->items[h->count++];

    it->id = id;
    it->row.icon = pw_assets_art(art);
    it->row.glow = glow >= 0 ? pw_assets_art(glow) : 0;
    it->row.fallback = fallback;
    it->row.title = title;
    it->row.enabled = enabled;
    stbsp_snprintf(it->desc, DESC_MAX, "%s", desc);
    it->row.desc = it->desc[0] ? it->desc : 0;
}

static item *find(hub *h, int id)
{
    for (int i = 0; i < h->count; i++)
        if (h->items[i].id == id)
            return &h->items[i];
    return 0;
}

static void build(hub *h)
{
    char line[DESC_MAX];
    int has = pw_engine_has_state();

    h->cat.icon = pw_assets_art(PW_ART_CAT_GAME);
    h->cat.fallback = PW_ICON_WATCH;
    h->cat.label = "Game";
    h->count = 0;
    add(h, IT_RESUME, PW_ART_UMD, PW_ART_UMD_GLOW, PW_ICON_RESUME, "Resume Game", 1, "");
    add(h, IT_SAVE, PW_ART_FOLDER, PW_ART_FOLDER_GLOW, PW_ICON_SAVE, "Save State", 1,
        has ? "Replaces the saved state." : "");
    if (has && h->saved_at[0])
        stbsp_snprintf(line, sizeof(line), "Saved at %s", h->saved_at);
    else
        stbsp_snprintf(line, sizeof(line), "%s", has ? "" : "No saved state");
    add(h, IT_LOAD, PW_ART_UPDATE, PW_ART_UPDATE_GLOW, PW_ICON_LOAD, "Load State", has, line);
    add(h, IT_QUIT, -1, -1, PW_ICON_QUIT, "Quit Game", 1, "");
#ifdef PW_DEBUG
    add(h, IT_CARD, PW_ART_MS, PW_ART_MS_GLOW, PW_ICON_CARD, "Memory Stick Test", 1,
        "Debug: test T1");
#endif
}

static void refresh_status(hub *h, int force)
{
    unsigned int now = pw_ui_now_ms();

    if (!force && now - h->t_status < STATUS_MS)
        return;
    h->t_status = now;
    pw_sysinfo_clock(h->clock, sizeof(h->clock));
    h->battery = pw_sysinfo_battery(&h->charging);
}

static void draw(hub *h, int show)
{
    pw_hub_row rows[IT_COUNT];
    pw_canvas *c = pw_fb_begin(show, pw_ui_now_ms());

    for (int i = 0; i < h->count; i++)
        rows[i] = h->items[i].row;
    pw_view_status(c, &h->ui, h->clock, h->battery, h->charging, show);
    pw_view_hub(c, &h->ui, &h->cat, rows, h->count, h->selpos, show);
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

static int do_save(hub *h)
{
    unsigned int t0 = pw_ui_now_ms(), ms;
    item *it;
    int ret;

    pw_dialog_busy(&h->ui, "Saving...");
    pw_engine_set_presuspend(pw_fb_hide);
    ret = pw_engine_save_held(h->frozen);
    pw_engine_set_presuspend(0);
    if (ret == PW_ENGINE_LOADED)
        return ret;
    pw_fb_recapture();
    pw_ui_flush_input(&h->ui);
    if (ret == PW_ENGINE_SAVED) {
        ms = pw_ui_now_ms() - t0;
        clock_hm(h->saved_at, sizeof(h->saved_at));
        build(h);
        if ((it = find(h, IT_SAVE)) != 0) {
            stbsp_snprintf(it->desc, DESC_MAX, "Saved at %s (%u.%u s)", h->saved_at, ms / 1000,
                           ms % 1000 / 100);
            it->row.desc = it->desc;
        }
    } else {
        build(h);
        pw_dialog_message(&h->ui,
                          ret == PW_ENGINE_BUSY ? "Another save or load is running."
                                                : "The state could not be saved.",
                          "Details are in pocketwatch.log.");
    }
    return ret;
}

static void do_load(hub *h)
{
    if (!pw_dialog_confirm(&h->ui, "Load the saved state?", "Unsaved progress will be lost.", 0))
        return;
    pw_dialog_busy(&h->ui, "Loading...");
    pw_engine_set_presuspend(pw_fb_hide);
    pw_engine_load_held(h->frozen); /* returns only on failure */
    pw_engine_set_presuspend(0);
    pw_fb_recapture();
    build(h);
    pw_dialog_message(&h->ui, "The state could not be loaded.", "Details are in pocketwatch.log.");
}

#ifdef PW_DEBUG
/* T1 (docs/research/m3-menu-e3.md): save, write a probe file, load, check it after the rollback. */
static void do_card_test(hub *h)
{
    char l1[64], l2[64];
    int ret;

    if (!pw_dialog_confirm(&h->ui, "Run the Memory Stick test (T1)?",
                           "It saves, writes a test file and loads.", 0))
        return;
    if (pw_cardtest_prepare() < 0) {
        pw_dialog_message(&h->ui, "The test could not start.", "Details are in pocketwatch.log.");
        return;
    }
    ret = do_save(h);
    if (ret == PW_ENGINE_LOADED) { /* back in the saved moment: the check has run */
        pw_cardtest_result(l1, l2, sizeof(l1));
        pw_fb_recapture();
        h->saved_at[0] = 0;
        build(h);
        pw_dialog_message(&h->ui, l1, l2);
        return;
    }
    if (ret != PW_ENGINE_SAVED) {
        pw_cardtest_cancel();
        return;
    }
    if (pw_cardtest_write_probe() < 0) {
        pw_cardtest_cancel();
        pw_dialog_message(&h->ui, "The test file could not be written.",
                          "Details are in pocketwatch.log.");
        return;
    }
    pw_dialog_busy(&h->ui, "Loading...");
    pw_engine_set_presuspend(pw_fb_hide);
    pw_engine_load_held(h->frozen); /* returns only on failure */
    pw_engine_set_presuspend(0);
    pw_cardtest_cancel();
    pw_fb_recapture();
    build(h);
    pw_dialog_message(&h->ui, "The load for the test failed.", "Details are in pocketwatch.log.");
}
#endif

int pw_hub_run(const pw_freeze_set *frozen)
{
    static hub h; /* keeps saved_at across openings; the thread's stack stays small */
    int result = PW_HUB_CLOSE;

    h.frozen = frozen;
    pw_ui_init(&h.ui);
    if (!pw_engine_has_state())
        h.saved_at[0] = 0;
    build(&h);
    refresh_status(&h, 1);
    h.sel = g_last_sel < h.count ? g_last_sel : 0;
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
        } else if (ev & h.ui.enter) {
            const item *it = &h.items[h.sel];

            g_last_sel = h.sel;
            if (!it->row.enabled)
                continue;
            if (it->id == IT_RESUME)
                break;
            if (it->id == IT_QUIT) {
                if (pw_dialog_confirm(&h.ui, "Do you want to quit the game?", 0, 0)) {
                    result = PW_HUB_QUIT;
                    break;
                }
            } else if (it->id == IT_SAVE) {
                pw_engine_set_load_hook(0);
                if (do_save(&h) == PW_ENGINE_LOADED)
                    return PW_HUB_LOADED; /* back in the saved moment; the screen is gone */
            } else if (it->id == IT_LOAD) {
                do_load(&h);
            }
#ifdef PW_DEBUG
            else if (it->id == IT_CARD) {
                do_card_test(&h);
            }
#endif
            refresh_status(&h, 1);
        }
    }
    g_last_sel = h.sel;
    fade(&h, 256, 0, FADE_OUT_MS);
    return result;
}

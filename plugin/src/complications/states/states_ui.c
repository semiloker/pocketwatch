// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * The States complication in the hub: Save State and Load State, the slot lists, the card writes
 * and the guards (docs/subsystems/states-storage.md, game-shell.md M3b).
 */
#include <psprtc.h>
#include <string.h>

#include "complications/complication.h"
#include "complications/states/engine.h"
#include "complications/states/store.h"
#include "core/game.h"
#include "core/log.h"
#include "core/platform.h"
#include "gfx/assets.h"
#include "shell/guard.h"
#include "stb_sprintf.h"
#include "ui/dialog.h"
#include "ui/fb.h"
#include "ui/i18n.h"
#include "ui/icons.h"
#include "ui/slots.h"
#ifdef PW_DEBUG
#include "debug/cardtest.h"
#endif

enum { ROW_SAVE, ROW_LOAD };

static struct {
    char saved_at[16];    /* "14:05" of the state held, or "" */
    int slots_used;       /* states on the Memory Stick for this game */
    int recount;          /* slots_used may be stale: count again when the hub opens */
    int last_slot;        /* the slot used last (cursor of the state list) */
    int guard;            /* PW_GUARD_* when the rows were built */
    const char *busy_fmt; /* "Saving... %d%%" in the current language */
    pw_ui *ui;            /* for the progress callback */
    int continue_slot;    /* a slot to load as soon as the game runs (continue.txt), or -1 */
} g = {.continue_slot = -1};

static void clock_hm(char *out, unsigned int cap)
{
    ScePspDateTime t;

    if (sceRtcGetCurrentClockLocalTime(&t) < 0)
        stbsp_snprintf(out, (int)cap, "--:--");
    else
        stbsp_snprintf(out, (int)cap, "%d:%02d", t.hour, t.minute);
}

static int count_slots(void)
{
    pw_slot_info info[PW_STORE_SLOTS];
    int n = 0;

    if (!pw_store_available())
        return 0;
    pw_store_scan(info, 0);
    for (int i = 0; i < PW_STORE_SLOTS; i++)
        n += info[i].exists;
    return n;
}

/*
 * continue.txt next to the PRX, "<disc ID> <slot 1-5>": continue this game from that slot as soon
 * as it runs (VSH-REQ-003; the XMB side will write it, docs/subsystems/vsh-integration.md). Read
 * and removed at plugin start, so it acts once whatever happens next.
 */
static void read_continue(void)
{
    char dir[200], path[256], text[48], id[16];
    int n, slot = 0, j = 0, i = 0;

    if (pw_log_dir(dir, sizeof(dir)) < 0)
        stbsp_snprintf(dir, sizeof(dir), "ms0:/seplugins/pocketwatch/");
    stbsp_snprintf(path, sizeof(path), "%scontinue.txt", dir);
    n = pw_file_read(path, text, sizeof(text) - 1);
    if (n <= 0)
        return;
    pw_file_remove(path);
    text[n] = 0;
    while (text[i] == ' ' || text[i] == '\t')
        i++;
    while (text[i] && text[i] != ' ' && text[i] != '\t' && text[i] != '\r' && text[i] != '\n' &&
           j < (int)sizeof(id) - 1) {
        if (text[i] != '-')
            id[j++] = text[i];
        i++;
    }
    id[j] = 0;
    while (text[i] == ' ' || text[i] == '\t')
        i++;
    while (text[i] >= '0' && text[i] <= '9')
        slot = slot * 10 + (text[i++] - '0');
    if (strcmp(id, pw_game_get()->id) != 0 || slot < 1 || slot > PW_STORE_SLOTS) {
        pw_log("states: continue.txt asks for %s slot %d; this is %s: ignored", id, slot,
               pw_game_get()->id);
        return;
    }
    g.continue_slot = slot - 1;
    pw_log("states: continue from slot %d once the game runs", slot);
}

static void states_init(void)
{
    pw_store_init(); /* the slots' hashed PARAM.SFO, while no game thread holds anything */
    /* Counted once here and after each of our own changes, not at every HOME: five reads of the
       card made the hub open late. Nothing else changes our slots while the game runs. */
    g.slots_used = count_slots();
    read_continue();
}

static void states_shutdown(void)
{
    pw_store_shutdown();
}

static void states_open(void)
{
    if (!pw_engine_has_state())
        g.saved_at[0] = 0;
    if (g.recount) {
        g.recount = 0;
        g.slots_used = count_slots();
    }
}

static void row(pw_cpl_item *it, int art, int glow, int fallback, int title, int enabled)
{
    it->art = art;
    it->glow = glow;
    it->fallback = fallback;
    it->title = pw_tr(title);
    it->enabled = enabled;
}

static int states_items(pw_cpl_item *out, int max)
{
    int has = pw_engine_has_state(), held = pw_store_held_slot(), any = has || g.slots_used > 0;

    if (max < 2)
        return 0;
    g.guard = pw_guard_states();
    row(&out[ROW_SAVE], PW_ART_FOLDER, PW_ART_FOLDER_GLOW, PW_ICON_SAVE, PW_S_SAVE_STATE, !g.guard);
    if (g.guard)
        stbsp_snprintf(out[ROW_SAVE].desc, PW_CPL_DESC_MAX, "%s", pw_guard_reason(g.guard));
    else if (pw_store_available())
        stbsp_snprintf(out[ROW_SAVE].desc, PW_CPL_DESC_MAX, pw_tr(PW_S_SLOTS_USED), g.slots_used,
                       PW_STORE_SLOTS);
    else if (has)
        stbsp_snprintf(out[ROW_SAVE].desc, PW_CPL_DESC_MAX, "%s", pw_tr(PW_S_REPLACES_RAM));

    row(&out[ROW_LOAD], PW_ART_UPDATE, PW_ART_UPDATE_GLOW, PW_ICON_LOAD, PW_S_LOAD_STATE,
        !g.guard && any);
    if (g.guard && any)
        stbsp_snprintf(out[ROW_LOAD].desc, PW_CPL_DESC_MAX, "%s", pw_guard_reason(g.guard));
    else if (has && held >= 0 && g.saved_at[0])
        stbsp_snprintf(out[ROW_LOAD].desc, PW_CPL_DESC_MAX, pw_tr(PW_S_SLOT_SAVED_AT), held + 1,
                       g.saved_at);
    else if (has && g.saved_at[0])
        stbsp_snprintf(out[ROW_LOAD].desc, PW_CPL_DESC_MAX, pw_tr(PW_S_SAVED_AT), g.saved_at);
    else if (!any)
        stbsp_snprintf(out[ROW_LOAD].desc, PW_CPL_DESC_MAX, "%s", pw_tr(PW_S_NO_SAVED_STATE));
    return 2;
}

static int states_changed(void)
{
    return pw_guard_states() != g.guard; /* the battery ran low, USB came on, ... */
}

static void progress(int percent, void *ctx)
{
    char text[48];

    (void)ctx;
    stbsp_snprintf(text, sizeof(text), g.busy_fmt, percent);
    pw_dialog_busy(g.ui, text);
}

/* After the engine saved into RAM: write the state to its slot on the Memory Stick. */
static void save_to_slot(pw_ui *ui, int slot)
{
    int ret;

    g.busy_fmt = pw_tr(PW_S_SAVING_PCT);
    g.ui = ui;
    ret = pw_store_save(slot, pw_fb_game(), PW_FB_STRIDE, progress, 0);
    pw_ui_flush_input(ui);
    g.slots_used = count_slots();
    if (ret < 0)
        pw_dialog_message(ui, pw_tr(PW_S_WRITE_FAILED), pw_store_reason(ret));
}

/* Save into RAM. Returns PW_ENGINE_SAVED, PW_ENGINE_LOADED (back in the saved moment) or < 0. */
static int do_save(const pw_cpl_env *env)
{
    int ret;

    pw_dialog_busy(env->ui, pw_tr(PW_S_SAVING));
    pw_engine_set_presuspend(pw_fb_hide);
    ret = pw_engine_save_held(env->frozen);
    pw_engine_set_presuspend(0);
    if (ret == PW_ENGINE_LOADED)
        return ret;
    pw_fb_recapture();
    pw_ui_flush_input(env->ui);
    if (ret == PW_ENGINE_SAVED)
        clock_hm(g.saved_at, sizeof(g.saved_at));
    else
        pw_dialog_message(env->ui,
                          ret == PW_ENGINE_BUSY ? pw_tr(PW_S_BUSY) : pw_tr(PW_S_SAVE_FAILED),
                          pw_tr(PW_S_DETAILS_LOG));
    return ret;
}

static int run_save(const pw_cpl_env *env)
{
    int slot = -1, ret;

    if (pw_store_available()) {
        slot = pw_slots_pick(env->ui, PW_SLOTS_SAVE, g.last_slot);
        if (slot < 0) {
            g.slots_used = count_slots(); /* the list may have deleted some */
            return PW_CPL_STAY;
        }
        g.last_slot = slot;
    }
    pw_engine_set_load_hook(pw_store_after_load); /* part of the snapshot */
    ret = do_save(env);
    if (ret == PW_ENGINE_LOADED) { /* back in the saved moment */
        g.recount = 1; /* this world's count is from the save; the card may have changed since */
        if (pw_store_check_failed()) {
            pw_fb_recapture();
            g.saved_at[0] = 0;
            pw_dialog_message(env->ui, pw_tr(PW_S_CHECK_FAILED), pw_tr(PW_S_CHECK_FAILED2));
        }
        return PW_CPL_LOADED;
    }
    if (ret == PW_ENGINE_SAVED && slot >= 0)
        save_to_slot(env->ui, slot);
    return PW_CPL_STAY;
}

static void load_slot(const pw_cpl_env *env, int slot);

static void run_load(const pw_cpl_env *env)
{
    int slot = -1;

    if (pw_store_available() && g.slots_used > 0) {
        slot = pw_slots_pick(env->ui, PW_SLOTS_LOAD, g.last_slot);
        g.slots_used = count_slots();
        if (slot < 0)
            return;
        g.last_slot = slot;
    }
    if (!pw_dialog_confirm(env->ui, pw_tr(PW_S_CONFIRM_LOAD), pw_tr(PW_S_CONFIRM_LOAD2), 0))
        return;
    load_slot(env, slot);
}

/* Read slot (or use the state in RAM: slot < 0) and roll back; returns only when that failed. */
static void load_slot(const pw_cpl_env *env, int slot)
{
    int ret;

    if (slot >= 0) {
        g.busy_fmt = pw_tr(PW_S_LOADING_PCT);
        g.ui = env->ui;
        ret = pw_store_load(slot, progress, 0);
        if (ret < 0) {
            pw_dialog_message(env->ui, pw_tr(PW_S_CANNOT_LOAD), pw_store_reason(ret));
            return;
        }
    }
    pw_store_arm(slot);
    pw_dialog_busy(env->ui, pw_tr(PW_S_LOADING));
    pw_engine_set_presuspend(pw_fb_hide);
    pw_engine_load_held(env->frozen); /* returns only on failure */
    pw_engine_set_presuspend(0);
    pw_fb_recapture();
    pw_dialog_message(env->ui, pw_tr(PW_S_LOAD_FAILED), pw_tr(PW_S_DETAILS_LOG));
}

static int states_startup_pending(void)
{
    return g.continue_slot >= 0;
}

static int states_startup(const pw_cpl_env *env)
{
    int slot = g.continue_slot, guard = pw_guard_states();

    g.continue_slot = -1; /* once, whatever happens */
    if (guard) {
        pw_dialog_message(env->ui, pw_guard_reason(guard), pw_guard_hint(guard));
        return PW_CPL_STAY;
    }
    pw_log("states: continuing from slot %d", slot + 1);
    g.slots_used = count_slots();
    g.last_slot = slot;
    load_slot(env, slot);
    return PW_CPL_STAY; /* only reached when the load failed: the game goes on from its start */
}

static int states_run(int index, const pw_cpl_env *env)
{
    int any = pw_engine_has_state() || g.slots_used > 0;

    if (g.guard) { /* the rows are disabled: say why */
        pw_dialog_message(env->ui, pw_guard_reason(g.guard), pw_guard_hint(g.guard));
        return PW_CPL_STAY;
    }
    if (index == ROW_SAVE)
        return run_save(env);
    if (any)
        run_load(env);
    return PW_CPL_STAY;
}

const pw_complication pw_cpl_states = {.id = "states",
                                       .init = states_init,
                                       .shutdown = states_shutdown,
                                       .on_open = states_open,
                                       .items = states_items,
                                       .run = states_run,
                                       .changed = states_changed,
                                       .startup_pending = states_startup_pending,
                                       .startup = states_startup};

#ifdef PW_DEBUG
/* T1 (docs/research/m3-menu-e3.md): save, write a probe file, load, check it after the rollback. */
static int card_items(pw_cpl_item *out, int max)
{
    if (max < 1)
        return 0;
    out[0].art = PW_ART_MS;
    out[0].glow = PW_ART_MS_GLOW;
    out[0].fallback = PW_ICON_CARD;
    out[0].title = "Memory Stick Test"; /* debug only: not translated */
    out[0].enabled = 1;
    stbsp_snprintf(out[0].desc, PW_CPL_DESC_MAX, "Debug: test T1");
    return 1;
}

static int card_run(int index, const pw_cpl_env *env)
{
    char l1[64], l2[64];
    int ret;

    (void)index;
    if (!pw_dialog_confirm(env->ui, "Run the Memory Stick test (T1)?",
                           "It saves, writes a test file and loads.", 0))
        return PW_CPL_STAY;
    if (pw_cardtest_prepare() < 0) {
        pw_dialog_message(env->ui, "The test could not start.", "Details are in pocketwatch.log.");
        return PW_CPL_STAY;
    }
    ret = do_save(env);
    if (ret == PW_ENGINE_LOADED) { /* back in the saved moment: the check has run */
        pw_cardtest_result(l1, l2, sizeof(l1));
        pw_fb_recapture();
        g.saved_at[0] = 0;
        pw_dialog_message(env->ui, l1, l2);
        return PW_CPL_STAY;
    }
    if (ret != PW_ENGINE_SAVED) {
        pw_cardtest_cancel();
        return PW_CPL_STAY;
    }
    if (pw_cardtest_write_probe() < 0) {
        pw_cardtest_cancel();
        pw_dialog_message(env->ui, "The test file could not be written.",
                          "Details are in pocketwatch.log.");
        return PW_CPL_STAY;
    }
    pw_dialog_busy(env->ui, "Loading...");
    pw_engine_set_presuspend(pw_fb_hide);
    pw_engine_load_held(env->frozen); /* returns only on failure */
    pw_engine_set_presuspend(0);
    pw_cardtest_cancel();
    pw_fb_recapture();
    pw_dialog_message(env->ui, "The load for the test failed.", "Details are in pocketwatch.log.");
    return PW_CPL_STAY;
}

const pw_complication pw_cpl_cardtest = {.id = "cardtest", .items = card_items, .run = card_run};
#endif

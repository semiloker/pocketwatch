// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspctrl.h>
#include <pspctrl_kernel.h>
#include <pspdisplay.h>
#include <pspge.h>
#include <pspkernel.h>
#include <psppower.h>

#include "complications/states/engine.h"
#include "core/freeze.h"
#include "core/log.h"
#include "stb_sprintf.h"
#include "ui/draw.h"
#include "ui/fb.h"
#include "ui/input.h"
#include "ui/menu.h"

#define POLL_US         20000
#define FREEZE_MS       500
#define GE_WAIT_MS      100
#define RELEASE_WAIT_MS 3000
#define GAME_BUTTONS    0x0000FFFFu /* what the game can see; masked while the menu is open */
#define ANY_BUTTON      (GAME_BUTTONS | PSP_CTRL_HOME | PSP_CTRL_NOTE)
#define DPAD            (PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT)

#define C_TEXT  PW_RGB(240, 240, 240)
#define C_DIM   PW_RGB(140, 140, 140)
#define C_PANEL PW_RGB(0, 0, 0)

enum { ITEM_SAVE, ITEM_LOAD, ITEM_RESUME, ITEMS };

static const char *const k_items[ITEMS] = {"Save state", "Load state", "Resume game"};

static SceUID g_thread = -1;
static volatile int g_stop, g_open;

static unsigned int now_us(void)
{
    return sceKernelGetSystemTimeLow();
}

static unsigned int buttons(void)
{
    SceCtrlData pad;

    return sceCtrlPeekBufferPositive(&pad, 1) < 0 ? 0 : pad.Buttons;
}

/* With the game frozen, let the graphics engine finish its list before we read the frame. */
static void wait_ge(void)
{
    unsigned int t0 = now_us();

    while (now_us() - t0 < GE_WAIT_MS * 1000) {
        int st = sceGeDrawSync(1);

        if (st < 0 || st == PSP_GE_LIST_DONE || st == PSP_GE_LIST_STALL_REACHED)
            return;
        sceKernelDelayThread(1000);
    }
}

static void draw(int sel, const char *status)
{
    pw_canvas *c = pw_fb_begin();
    int has_state = pw_engine_has_state();
    int w;

    pw_draw_blend(c, 0, 44, 480, 184, C_PANEL, 150);
    pw_draw_fill(c, 0, 44, 480, 1, C_DIM);
    pw_draw_fill(c, 0, 227, 480, 1, C_DIM);
    w = pw_draw_text(c, 40, 58, "Pocketwatch", C_TEXT, 2);
    pw_draw_text(c, 40 + w + 12, 66, "Pause. Save. Play.", C_DIM, 1);

    for (int i = 0; i < ITEMS; i++) {
        int y = 98 + i * 30;
        unsigned int color = (i == ITEM_LOAD && !has_state) ? C_DIM : C_TEXT;

        if (i == sel) {
            pw_draw_blend(c, 28, y - 7, 424, 30, PW_RGB(255, 255, 255), 48);
            pw_draw_fill(c, 28, y - 7, 3, 30, C_TEXT);
        }
        pw_draw_text(c, 48, y, k_items[i], color, 2);
    }
    pw_draw_text(c, 40, 196, status, C_DIM, 1);
    pw_draw_text(c, 40, 244, "X select    O back", C_DIM, 1);
    pw_fb_present();
}

static void describe_state(char *status, unsigned int cap)
{
    stbsp_snprintf(status, (int)cap, "%s",
                   pw_engine_has_state() ? "A state is held in RAM." : "No state saved yet.");
}

/* Returns PW_ENGINE_LOADED when a load brought us back here: the menu must close at once. */
static int do_save(const pw_freeze_set *frozen, char *status, unsigned int cap)
{
    unsigned int t0 = now_us();
    int ret;

    pw_fb_hide(); /* the suspend overwrites P11, where our screen lives */
    ret = pw_engine_save_held(frozen);
    if (ret == PW_ENGINE_LOADED)
        return ret;
    pw_fb_recapture();
    if (ret == PW_ENGINE_SAVED) {
        unsigned int ms = (now_us() - t0) / 1000;

        stbsp_snprintf(status, (int)cap, "Saved in %u.%u s.", ms / 1000, (ms % 1000) / 100);
    } else {
        stbsp_snprintf(status, (int)cap, "Save failed (see the log).");
    }
    return ret;
}

static void do_load(const pw_freeze_set *frozen, char *status, unsigned int cap)
{
    if (!pw_engine_has_state()) {
        stbsp_snprintf(status, (int)cap, "No state to load.");
        return;
    }
    pw_fb_hide();
    pw_engine_load_held(frozen); /* returns only on failure */
    pw_fb_recapture();
    stbsp_snprintf(status, (int)cap, "Load failed (see the log).");
}

static void run_menu(void)
{
    pw_freeze_set frozen;
    pw_keys keys = {0, 0, 0};
    char status[64];
    int sel = 0, dirty = 1;
    unsigned int t_tick = 0;

    pw_freeze_game(&frozen, FREEZE_MS);
    if (frozen.count == 0) { /* no game running (or someone else froze it) */
        pw_thaw_game(&frozen);
        return;
    }
    g_open = 1;
    sceCtrlSetButtonMasks(GAME_BUTTONS, 1); /* the game reads no button until we close */
    wait_ge();
    if (pw_fb_open() < 0) {
        pw_log("menu: no memory for the screen");
        sceCtrlSetButtonMasks(GAME_BUTTONS, 0);
        g_open = 0;
        pw_thaw_game(&frozen);
        return;
    }
    describe_state(status, sizeof(status));
    keys.prev = buttons(); /* the tap that opened us is not a menu press */

    for (;;) {
        unsigned int ev;

        if (dirty) {
            draw(sel, status);
            dirty = 0;
        }
        sceDisplayWaitVblankStart();
        if (now_us() - t_tick > 1000000) { /* keep the backlight on */
            scePowerTick(0);
            t_tick = now_us();
        }
        ev = pw_keys_update(&keys, buttons(), DPAD, now_us());
        if (ev & PSP_CTRL_UP) {
            sel = (sel + ITEMS - 1) % ITEMS;
            dirty = 1;
        } else if (ev & PSP_CTRL_DOWN) {
            sel = (sel + 1) % ITEMS;
            dirty = 1;
        } else if (ev & (PSP_CTRL_CIRCLE | PSP_CTRL_START | PSP_CTRL_NOTE)) {
            break;
        } else if (ev & PSP_CTRL_CROSS) {
            if (sel == ITEM_RESUME)
                break;
            if (sel == ITEM_SAVE && do_save(&frozen, status, sizeof(status)) == PW_ENGINE_LOADED)
                break; /* back in the saved moment: close and let the game run from there */
            if (sel == ITEM_LOAD)
                do_load(&frozen, status, sizeof(status));
            keys.prev = buttons();
            dirty = 1;
        }
    }

    pw_fb_close();
    /* The game must not see the button that closed us: wait until everything is up. */
    for (unsigned int t0 = now_us();
         (buttons() & ANY_BUTTON) && now_us() - t0 < RELEASE_WAIT_MS * 1000;)
        sceKernelDelayThread(POLL_US);
    sceCtrlSetButtonMasks(GAME_BUTTONS, 0);
    g_open = 0;
    pw_thaw_game(&frozen);
}

static int menu_thread(SceSize args, void *argp)
{
    pw_tap tap = {0, 0, 0};

    (void)args;
    (void)argp;
    while (!g_stop) {
        unsigned int b;

        sceKernelDelayThread(POLL_US);
        b = buttons();
        if (pw_tap_update(&tap, (b & PSP_CTRL_NOTE) != 0, (b & (GAME_BUTTONS | PSP_CTRL_HOME)) != 0,
                          now_us()))
            run_menu();
    }
    return 0;
}

int pw_menu_start(void)
{
    g_stop = 0;
    g_thread = sceKernelCreateThread("PocketwatchMenu", menu_thread, 0x20, 0x2000, 0, NULL);
    if (g_thread < 0)
        return g_thread;
    return sceKernelStartThread(g_thread, 0, NULL);
}

void pw_menu_stop(void)
{
    SceUInt timeout = 500 * 1000;

    if (g_thread < 0)
        return;
    g_stop = 1;
    if (sceKernelWaitThreadEnd(g_thread, &timeout) < 0)
        sceKernelTerminateThread(g_thread);
    sceKernelDeleteThread(g_thread);
    g_thread = -1;
}

int pw_menu_is_open(void)
{
    return g_open;
}

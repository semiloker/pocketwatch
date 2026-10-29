// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspctrl.h>
#include <pspctrl_kernel.h>
#include <pspge.h>
#include <pspkernel.h>
#include <psploadcore.h>

#include "complications/registry.h"
#include "core/config.h"
#include "complications/states/engine.h"
#include "core/freeze.h"
#include "core/log.h"
#include "core/platform.h"
#include "core/sysinfo.h"
#include "gfx/assets.h"
#include "core/sysctrl.h"
#include "shell/shell.h"
#include "ui/fb.h"
#include "ui/hub.h"
#include "ui/input.h"

#define POLL_US           20000
#define FREEZE_MS         500
#define GE_WAIT_MS        100
#define RELEASE_WAIT_MS   3000
#define TAKEOVER_MAX_MS   30000       /* give up waiting for the game's exit callback */
#define RESUME_QUIET_MS   2000        /* no hub right after a resume (Custom Home does the same) */
#define RECHECK_MS        500         /* someone may switch the system HOME screen back on */
#define QUIT_WAIT_MS      5000        /* the game's own exit callback, then ARK's way */
#define STARTUP_SETTLE_MS 1500        /* a start-up action waits this long after the takeover */
#define STARTUP_MAX_MS    30000       /* and gives up when the screen could not open for so long */
#define TICK_MS           1000        /* the complications' background tick */
#define GAME_BUTTONS      0x0000FFFFu /* what the game can see; masked while the hub is open */
#define ANY_BUTTON        (GAME_BUTTONS | PSP_CTRL_HOME | PSP_CTRL_NOTE)

/* core/stubs: sceImpose_driver, LoadExecForKernel, LoadCoreForKernel */
int sceImposeSetHomePopup(int value);
int sceImposeGetHomePopup(void);
int sceKernelCheckExitCallback(void);
int sceKernelInvokeExitCallback(void);

enum { HOME_WAITING, HOME_OWNED, HOME_NOT_OURS };

static SceUID g_thread = -1;
static volatile int g_stop, g_open, g_home = HOME_WAITING;

static unsigned int now_ms(void)
{
    return sceKernelGetSystemTimeLow() / 1000;
}

static unsigned int buttons(void)
{
    return pw_buttons_held(); /* not through the functions ARK-4 hooks for its exit combos */
}

/* A system dialog of the game (save, message, keyboard, network) is up: HOME is the system's. */
static int system_dialog_up(void)
{
    static const char *const names[] = {"sceVshSDUtility_Module", "sceDialogmain_Module",
                                        "sceVshOSK_Module", "sceVshNetconf_Module"};

    for (unsigned int i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        if (sceKernelFindModuleByName(names[i]) != NULL)
            return 1;
    return 0;
}

static void take_home(unsigned int start_ms, unsigned int b)
{
    int popup;

    if (now_ms() - start_ms > TAKEOVER_MAX_MS) {
        g_home = HOME_NOT_OURS;
        pw_log("shell: the game registered no exit callback in %u s: NOTE opens the menu",
               TAKEOVER_MAX_MS / 1000);
        return;
    }
    if (sceKernelCheckExitCallback() == 0 || (b & PSP_CTRL_HOME))
        return; /* the game is not ready yet, or HOME is held right now */
    popup = sceImposeGetHomePopup();
    if (popup == 0) {
        g_home = HOME_NOT_OURS;
        pw_log("shell: the HOME screen is already off (the game or a plugin owns HOME): NOTE "
               "opens the menu");
        return;
    }
    sceImposeSetHomePopup(0);
    g_home = HOME_OWNED;
    pw_log("shell: HOME opens Pocketwatch (system HOME screen was %d, now %d)", popup,
           sceImposeGetHomePopup());
}

static void wait_ge(void)
{
    unsigned int t0 = now_ms();

    while (now_ms() - t0 < GE_WAIT_MS) {
        int st = sceGeDrawSync(1);

        if (st < 0 || st == PSP_GE_LIST_DONE || st == PSP_GE_LIST_STALL_REACHED)
            return;
        sceKernelDelayThread(1000);
    }
}

static void quit_game(void)
{
    int (*hold_mode)(int);

    pw_log("shell: Quit Game");
    pw_engine_prepare_exit(); /* drops any state, checks the IPL seeds, flushes the log */
    pw_registry_quit(pw_complications, pw_complication_count); /* their last card writes */
    if (sceKernelCheckExitCallback() != 0) {
        /* The system HOME screen's own way: the game's exit callback ends the game. */
        int ret = sceKernelInvokeExitCallback();

        pw_log("shell: quit: the game's exit callback was invoked (0x%08X)", ret);
        for (unsigned int t0 = now_ms(); !g_stop && now_ms() - t0 < QUIT_WAIT_MS;)
            sceKernelDelayThread(50 * 1000);
        if (g_stop)
            return; /* the game is exiting: module_stop is waiting for us */
        pw_log("shell: quit: the game did not exit in %u s: quitting through ARK",
               QUIT_WAIT_MS / 1000);
    }
    /* What ARK-4 does for its own exit combo (core/systemctrl/src/exitgame.c). */
    hold_mode = (int (*)(int))sctrlHENFindFunction("sceDisplay_Service", "sceDisplay", 0x7ED59BC4);
    if (hold_mode)
        hold_mode(0);
    sctrlSESetUmdFile(NULL);
    sctrlSESetBootConfFileIndex(0);
    sctrlKernelExitVSH(NULL);
}

/* Freeze the game, open the screen, run the hub (or a complication's start-up action). t_press:
   when HOME went down (us), for the log line that says where the time to the first frame went. */
static void open_screen(const pw_complication *startup, unsigned int t_press)
{
    pw_freeze_set frozen;
    unsigned int t_frozen, t_ge, t_screen;
    int result;

    pw_freeze_game(&frozen, FREEZE_MS);
    t_frozen = sceKernelGetSystemTimeLow();
    if (frozen.count == 0) { /* no game running (or someone else froze it) */
        pw_thaw_game(&frozen);
        if (frozen.card_busy)
            pw_log("shell: HOME ignored: the game kept the Memory Stick busy for %u ms", frozen.ms);
        return;
    }
    g_open = 1;
    sceCtrlSetButtonMasks(GAME_BUTTONS, 1); /* the game reads no button until we close */
    wait_ge();
    t_ge = sceKernelGetSystemTimeLow();
    char clock[24];
    int month = pw_sysinfo_clock(clock, sizeof(clock)), sky = pw_sysinfo_sky_index(month);
    static int logged_sky = -1;

    if (sky != logged_sky) { /* which of the twelve skies, and why (theme colour or month) */
        pw_log("shell: sky %d (theme colour %d, month %d)", sky, pw_sysinfo_get()->color_mode,
               month);
        logged_sky = sky;
    }
    if (pw_fb_open(pw_assets_sky(sky), pw_config_get()->style) < 0) {
        pw_log("shell: no memory for the screen");
        sceCtrlSetButtonMasks(GAME_BUTTONS, 0);
        g_open = 0;
        pw_thaw_game(&frozen);
        return;
    }

    t_screen = sceKernelGetSystemTimeLow();

    result = startup ? pw_hub_startup(&frozen, startup) : pw_hub_run(&frozen);

    if (pw_fb_first_frame_us())
        pw_log("shell: open: freeze %u ms (the card held %d time(s)), GE %u ms, screen %u ms, "
               "first frame %u ms after the press",
               (t_frozen - t_press) / 1000, frozen.card_tries, (t_ge - t_frozen) / 1000,
               (t_screen - t_ge) / 1000, (pw_fb_first_frame_us() - t_press) / 1000);
    pw_fb_close();
    /* The game must not see the button that closed us: wait until everything is up. */
    for (unsigned int t0 = now_ms(); (buttons() & ANY_BUTTON) && now_ms() - t0 < RELEASE_WAIT_MS;)
        sceKernelDelayThread(POLL_US);
    sceCtrlSetButtonMasks(GAME_BUTTONS, 0);
    g_open = 0;
    pw_thaw_game(&frozen);
    if (result == PW_HUB_QUIT)
        quit_game();
}

static void open_hub(unsigned int t_press)
{
    open_screen(0, t_press);
}

/* Why the screen cannot open now, or NULL. */
static const char *why_not_open(void)
{
    if (pw_engine_busy())
        return "a save or load is running";
    if (now_ms() - pw_engine_last_resume_ms() <= RESUME_QUIET_MS)
        return "the PSP has just resumed";
    if (system_dialog_up())
        return "a system dialog (saved data, keyboard, network) is open";
    return 0;
}

static int may_open(void)
{
    const char *why = why_not_open();

    if (why)
        pw_log("shell: HOME ignored: %s", why);
    return why == 0;
}

/*
 * Once per game, after it runs (its exit callback registered, or we gave up waiting for it): a
 * complication may have something to do at once (a state to continue from, VSH-REQ-003). It waits
 * a little first: games load their own modules right after registering the callback (INFERRED).
 */
static void run_startup(unsigned int ready_ms)
{
    static int done;
    const pw_complication *cpl;
    const char *why;

    if (done || now_ms() - ready_ms < STARTUP_SETTLE_MS)
        return;
    cpl = pw_registry_startup(pw_complications, pw_complication_count);
    if (!cpl) {
        done = 1;
        return;
    }
    why = why_not_open();
    if (why && now_ms() - ready_ms < STARTUP_MAX_MS)
        return; /* try again on the next poll */
    done = 1;
    if (why) {
        pw_log("shell: %s's start-up action dropped: %s", cpl->id, why);
        return;
    }
    pw_log("shell: %s has a start-up action", cpl->id);
    open_screen(cpl, sceKernelGetSystemTimeLow());
}

static int shell_thread(SceSize args, void *argp)
{
    pw_tap tap = {0, 0, 0};
    unsigned int start = now_ms(), last_check = 0, ready = 0, t_poll, last_tick = 0;
    int home_was_down = 0;

    (void)args;
    (void)argp;
    while (!g_stop) {
        unsigned int b;

        sceKernelDelayThread(POLL_US);
        b = buttons();
        t_poll = sceKernelGetSystemTimeLow();
        if (now_ms() - last_tick >= TICK_MS && !pw_engine_busy()) {
            last_tick = now_ms();
            pw_registry_tick(pw_complications, pw_complication_count, last_tick);
        }
        if (g_home == HOME_WAITING) {
            take_home(start, b);
            ready = now_ms();
        } else {
            run_startup(ready); /* the game runs: its exit callback is there, or we gave up */
        }
        if (g_home == HOME_OWNED) {
            if (now_ms() - last_check > RECHECK_MS) {
                last_check = now_ms();
                if (sceImposeGetHomePopup() != 0) {
                    sceImposeSetHomePopup(0);
                    pw_log("shell: the system HOME screen was switched back on: off again");
                }
            }
            /* HOME is kernel-only: the game never sees it. Open as it goes down, like the system's
               HOME screen; the menu takes the held HOME as handled, so its release does nothing
               and the next press closes. After the menu, HOME must come up before it opens again.
             */
            int home = (b & PSP_CTRL_HOME) != 0;

            if (home && !home_was_down && may_open())
                open_hub(t_poll);
            home_was_down = home;
        } else if (pw_tap_update(&tap, (b & PSP_CTRL_NOTE) != 0,
                                 (b & (GAME_BUTTONS | PSP_CTRL_HOME)) != 0,
                                 sceKernelGetSystemTimeLow()) &&
                   may_open()) {
            open_hub(t_poll);
        }
    }
    return 0;
}

int pw_shell_start(void)
{
    g_stop = 0;
    g_thread = sceKernelCreateThread("PocketwatchShell", shell_thread, 0x20, 0x4000, 0, NULL);
    if (g_thread < 0)
        return g_thread;
    return sceKernelStartThread(g_thread, 0, NULL);
}

void pw_shell_stop(void)
{
    SceUInt timeout = 500 * 1000;

    if (g_thread >= 0) {
        g_stop = 1;
        if (sceKernelWaitThreadEnd(g_thread, &timeout) < 0)
            sceKernelTerminateThread(g_thread);
        sceKernelDeleteThread(g_thread);
        g_thread = -1;
    }
    if (g_home == HOME_OWNED)
        sceImposeSetHomePopup(1); /* the system's HOME screen again */
    g_home = HOME_WAITING;
}

int pw_shell_is_open(void)
{
    return g_open;
}

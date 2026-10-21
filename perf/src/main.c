// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Pocketwatch's overlay module in games (PRF-REQ-001..008, ADR-043, ADR-045, docs/research/
 * m7-perf-overlay.md): the performance numbers, the volume and brightness indicator, the
 * brightness boost. A kernel module of its own, in ARK-4's PLUGINS.TXT only while one of them is
 * on in Home's Settings - the game plugin's kernel RAM (2 KB left, ADR-037) pays nothing, and a
 * game without it runs as before. One thread: at every vblank it writes into the frame the PSP
 * shows next (the hub's own while the hub is open, else the game's); every 6th it reads the
 * volume and the brightness; once a second it measures. It writes nothing to the card; it reads
 * perf.txt once. It runs ONLY in a real disc/ISO game (sceKernelInitApitype, like the game plugin,
 * core/game.c): in Home and the XMB the module loads but its thread never starts, so it cannot
 * touch the display during boot - an earlier VSH line ran it in the XMB and its framebuffer writes
 * crashed Home's start (ADR-049, 2026-10-21).
 */
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <pspthreadman_kernel.h>
#include <string.h>

#include "overlay.h"

PSP_MODULE_INFO("PocketwatchPerf", PSP_MODULE_KERNEL, 1, 0);

/* stubs.S */
int scePowerGetCpuClockFrequency(void);
int scePowerGetBusClockFrequency(void);
int scePowerGetBatteryLifePercent(void);
int scePowerIsBatteryExist(void);
int scePowerGetBatteryLifeTime(void);
int scePowerGetBatteryTemp(void);
int scePowerGetBatteryVolt(void);
int sceSysconBatteryGetElec(int *ma);
int sceDisplayGetFrameBuf(void **top, int *bufw, int *fmt, int sync);
int sceDisplayWaitVblankStart(void);
int sceDisplayGetFrameBufferInternal(int pri, void **top, int *bufw, int *fmt, int sync);
int sceCtrlPeekBufferPositive(SceCtrlData *pad, int count);
SceSize sceKernelPartitionTotalFreeMemSize(int pid);
int sceImposeGetParam(int param);
int sceDisplayGetBrightness(int *level, int *unk);
int sceDisplaySetBrightness(int level, int unk);
int sceKernelInitApitype(void);

/* Retail disc/ISO apitypes (ARK-4 runlevel "umd"); the game plugin's own check (core/game.c). */
static int is_disc_apitype(int a)
{
    return a == 0x120 || (a >= 0x123 && a <= 0x126) || a == 0x130 || a == 0x160 ||
           (a >= 0x110 && a <= 0x115);
}

#define CFG               "ms0:/seplugins/pocketwatch/perf.txt"
#define HOLD_MS           1000 /* L + R + SELECT held this long: the next mode */
#define ELEC_S            10 /* the current: a command the syscon sends to the battery, so not often */
#define COMBO             (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_SELECT)
#define OTHERS            0x0000F0F8u /* the d-pad, the face buttons, START: none of them with the combo */
#define SCREEN_W          480
#define SCREEN_H          272
#define AV_EVERY          6 /* vblanks between readings of the volume and the brightness (100 ms) */
#define GRACE_MS          2000 /* no drawing for the game's first seconds (its display is settling) */
#define IMPOSE_VOLUME     0x1 /* pspimpose_driver.h */
#define IMPOSE_BRIGHTNESS 0x2
#define IMPOSE_MUTE       0x8

static volatile int g_run = 1;
static SceUID g_thread = -1;

static int g_mode = PW_PERF_FULL;
static pw_perf_cfg g_cfg;
static pw_perf_mask g_mask[2], g_osd_mask;
static int g_lines;

static SceUID g_idle[2]; /* the idle threads: what they ran is the CPU left over */
static int g_nidle;

static unsigned int now_ms(void)
{
    return sceKernelGetSystemTimeLow() / 1000;
}

static void read_mode(void)
{
    char t[96];
    SceUID fd = sceIoOpen(CFG, PSP_O_RDONLY, 0);
    int n = fd >= 0 ? sceIoRead(fd, t, sizeof(t)) : 0;

    if (fd >= 0)
        sceIoClose(fd);
    pw_perf_cfg_parse(t, n > 0 ? n : 0, &g_cfg);
    g_mode = g_cfg.mode;
}

static void find_idle(void)
{
    SceUID ids[96];
    int n = 0;

    if (sceKernelGetThreadmanIdList(SCE_KERNEL_TMID_Thread, ids, 96, &n) < 0)
        return;
    for (int i = 0; i < n && i < 96 && g_nidle < 2; i++) {
        SceKernelThreadInfo info;

        memset(&info, 0, sizeof(info));
        info.size = sizeof(info);
        if (sceKernelReferThreadStatus(ids[i], &info) == 0 &&
            (strcmp(info.name, "idle0") == 0 || strcmp(info.name, "idle1") == 0))
            g_idle[g_nidle++] = ids[i];
    }
}

/* What the idle threads have run (us), or 0 when they are not known. */
static unsigned long long idle_us(void)
{
    unsigned long long us = 0;

    for (int i = 0; i < g_nidle; i++) {
        SceKernelThreadInfo info;

        memset(&info, 0, sizeof(info));
        info.size = sizeof(info);
        if (sceKernelReferThreadStatus(g_idle[i], &info) == 0)
            us += (unsigned long long)info.runClocks.hi << 32 | info.runClocks.low;
    }
    return us;
}

static void lines(const pw_perf_stats *s)
{
    char l[2][PW_PERF_COLS + 1];

    g_lines = pw_perf_lines(s, g_mode, l);
    for (int i = 0; i < g_lines; i++)
        pw_perf_render(l[i], &g_mask[i]);
}

/* The volume and the brightness (ADR-045, ADR-047): the indicator when one changed, the boost or
   Pocketwatch's steps kept. */
static pw_osd g_osd;
static pw_bright g_bright;
static pw_steps g_steps;
static pw_fbset g_fbs;          /* the game's display buffers: the overlay goes into all of them */
static int g_steps_set;         /* the first step taken from the level */
static int g_level = -1;        /* the backlight's level as last read */
static unsigned int g_t_screen; /* SCREEN held last (a change of the owner's) */

static int steps_n(void)
{
    return g_cfg.steps ? g_cfg.nlevels : 4;
}

static void av_step(unsigned int now)
{
    int level = 0, unk = 0, set, fixed = 0;

    if (sceDisplayGetBrightness(&level, &unk) < 0)
        level = -1;
    if (g_cfg.steps && !g_steps_set && level > 0) {
        g_steps.cur = pw_steps_nearest(&g_cfg, level);
        g_steps_set = 1;
    }
    if (g_cfg.steps)
        fixed = g_cfg.levels[g_steps.cur];
    g_level = level;
    if ((g_cfg.boost > 0 || fixed > 0) &&
        (set = pw_bright_step(&g_bright, level, g_cfg.boost, fixed, now - g_t_screen < 1000, now)) >
            0) {
        sceDisplaySetBrightness(set, unk);
        g_level = set;
    }
    if (g_cfg.osd == PW_OSD_ALL) {
        int bright = g_cfg.steps ? g_steps.cur : sceImposeGetParam(IMPOSE_BRIGHTNESS);

        if (pw_osd_step(&g_osd, sceImposeGetParam(IMPOSE_VOLUME), sceImposeGetParam(IMPOSE_MUTE),
                        bright, now)) {
            char t[PW_PERF_COLS + 1];

            pw_osd_text(&g_osd, g_level, steps_n(), t);
            pw_perf_render(t, &g_osd_mask);
        }
    }
}

/* hub: the hub's frame is shown - it has its own volume bar */
static void osd_draw(void *top, int bufw, int fmt, unsigned int now, int hub)
{
    int kind = pw_osd_shown(&g_osd, now);

    if (kind == PW_OSD_VOLUME && !hub)
        pw_osd_draw(top, bufw, fmt, SCREEN_W, SCREEN_H, &g_osd_mask,
                    g_osd.mute == 1 ? 0 : g_osd.vol, 30);
    else if (kind == PW_OSD_BRIGHT)
        pw_osd_draw(top, bufw, fmt, SCREEN_W, SCREEN_H, &g_osd_mask, g_osd.bright + 1, steps_n());
}

/* The numbers on a black box: the owner could not read them over a game's picture. */
static void lines_draw(void *top, int bufw, int fmt)
{
    int w = 0;

    for (int i = 0; i < g_lines; i++)
        w = g_mask[i].w > w ? g_mask[i].w : w;
    pw_perf_box(top, bufw, fmt, SCREEN_W, SCREEN_H, 2, 2, w + 6, g_lines * 11 + 4);
    for (int i = 0; i < g_lines; i++)
        pw_perf_draw(top, bufw, fmt, SCREEN_W, SCREEN_H, 5, 4 + i * 11, &g_mask[i]);
}

static int perf_thread(SceSize args, void *argp)
{
    pw_perf_stats s;
    unsigned int t_sec = now_ms(), t_hold = 0, held = 0, elec_at = 0, vblanks = 0, t0 = now_ms();
    unsigned long long idle0 = 0, wall0 = sceKernelGetSystemTimeWide();
    void *last = 0;
    int flips = 0;

    (void)args;
    (void)argp;
    read_mode();
    pw_bright_init(&g_bright);
    find_idle();
    idle0 = idle_us();
    memset(&s, 0, sizeof(s));
    s.fps = -1;
    s.cpu_pct = s.free_kb = s.bat_pct = s.bat_ma = s.bat_mv = s.bat_temp = s.bat_min = PW_PERF_NA;
    lines(&s);
    while (g_run) {
        void *game = 0, *top = 0;
        int bufw = 0, fmt = 0, gw = 0, gf = 0;
        unsigned int now;
        SceCtrlData pad;

        sceDisplayWaitVblankStart();
        now = now_ms();
        memset(&pad, 0, sizeof(pad));
        sceCtrlPeekBufferPositive(&pad, 1);
        /* SCREEN: a tap is Pocketwatch's next step (read every vblank: a tap is short) */
        if (pad.Buttons & PSP_CTRL_SCREEN)
            g_t_screen = now;
        if (g_cfg.steps &&
            pw_steps_button(&g_steps, (pad.Buttons & PSP_CTRL_SCREEN) != 0, g_cfg.nlevels, now))
            av_step(now);
        else if (++vblanks % AV_EVERY == 0 &&
                 (g_cfg.boost > 0 || g_cfg.steps || g_cfg.osd == PW_OSD_ALL))
            av_step(now);
        /* L + R + SELECT held a second, nothing else: the next mode (once per hold) */
        if ((pad.Buttons & COMBO) == COMBO && !(pad.Buttons & OTHERS)) {
            if (!t_hold)
                t_hold = now;
            else if (!held && now - t_hold >= HOLD_MS) {
                held = 1;
                g_mode = pw_perf_next(g_mode);
                lines(&s);
            }
        } else {
            t_hold = held = 0;
        }
        /* the game's frame: a new one shown is a frame of its rate */
        if (sceDisplayGetFrameBuf(&game, &gw, &gf, PSP_DISPLAY_SETBUF_IMMEDIATE) >= 0 && game &&
            game != last) {
            flips++;
            last = game;
        }
        if (now - t_sec >= 1000) {
            unsigned long long wall = sceKernelGetSystemTimeWide(), idle = idle_us();

            s.fps = flips > 0 ? flips * 1000 / (int)(now - t_sec) : -1;
            s.cpu_pct = g_nidle && wall > wall0 ? 100 - (int)((idle - idle0) * 100 / (wall - wall0))
                                                : PW_PERF_NA;
            s.cpu_mhz = scePowerGetCpuClockFrequency();
            s.bus_mhz = scePowerGetBusClockFrequency();
            s.free_kb = (int)(sceKernelPartitionTotalFreeMemSize(2) >> 10);
            if (scePowerIsBatteryExist() == 1) {
                int ma = 0;

                s.bat_pct = scePowerGetBatteryLifePercent();
                s.bat_mv = scePowerGetBatteryVolt();
                s.bat_temp = scePowerGetBatteryTemp();
                s.bat_min = scePowerGetBatteryLifeTime();
                if (g_mode == PW_PERF_FULL && (!elec_at || now - elec_at >= ELEC_S * 1000)) {
                    elec_at = now;
                    s.bat_ma = sceSysconBatteryGetElec(&ma) >= 0 ? ma : PW_PERF_NA;
                }
            }
            if (s.cpu_pct != PW_PERF_NA && (s.cpu_pct < 0 || s.cpu_pct > 100))
                s.cpu_pct = PW_PERF_NA;
            lines(&s);
            flips = 0;
            t_sec = now;
            idle0 = idle;
            wall0 = wall;
        }
        if (now - t0 < GRACE_MS) /* let the game set up its display first */
            continue;
        if ((g_mode == PW_PERF_OFF || !g_lines) && pw_osd_shown(&g_osd, now) == PW_OSD_NONE)
            continue;
        /* remember the game's buffers so the overlay goes into all of them (no flicker) */
        if (game && gw >= SCREEN_W)
            pw_fbset_add(&g_fbs, (unsigned int)game, gw, gf);
        /* the Pocketwatch hub's frame (priority 0, its own buffer) hides the game while it is
           open: then draw only on it; otherwise draw into every game buffer */
        if (sceDisplayGetFrameBufferInternal(0, &top, &bufw, &fmt, PSP_DISPLAY_SETBUF_IMMEDIATE) >=
                0 &&
            top && bufw >= SCREEN_W && top != game) {
            void *t = (void *)((unsigned int)top | 0x40000000u);

            if (g_mode != PW_PERF_OFF && g_lines)
                lines_draw(t, bufw, fmt);
            osd_draw(t, bufw, fmt, now, 1);
        } else {
            for (int i = 0; i < g_fbs.n; i++) {
                void *t = (void *)(g_fbs.fb[i] | 0x40000000u); /* uncached: nothing to write back */

                if (g_mode != PW_PERF_OFF && g_lines)
                    lines_draw(t, g_fbs.w[i], g_fbs.f[i]);
                osd_draw(t, g_fbs.w[i], g_fbs.f[i], now, 0);
            }
        }
    }
    return 0;
}

int module_start(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    /* Only in a real game: in Home and the XMB the thread never runs, so it cannot touch the
       display during boot (ADR-049). */
    if (!is_disc_apitype(sceKernelInitApitype()))
        return 0;
    g_thread = sceKernelCreateThread("PocketwatchPerf", perf_thread, 0x10, 0x2000, 0, 0);
    if (g_thread >= 0)
        sceKernelStartThread(g_thread, 0, 0);
    return 0;
}

int module_stop(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    g_run = 0;
    if (g_thread >= 0)
        sceKernelWaitThreadEnd(g_thread, 0);
    return 0;
}

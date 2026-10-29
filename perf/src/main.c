// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Pocketwatch's overlay module in games (PRF-REQ-001..008, ADR-043, ADR-045, docs/research/
 * m7-perf-overlay.md): the performance numbers, the volume and brightness indicator, the
 * brightness boost. A kernel module of its own, in ARK-4's PLUGINS.TXT only while one of them is
 * on in Home's Settings - the game plugin's kernel RAM (2 KB left, ADR-037) pays nothing, and a
 * game without it runs as before. One thread: at every vblank it writes into the frame the PSP
 * shows next (the hub's own while the hub is open, else the game's); every 6th it reads the
 * volume and the brightness; once a second it measures. It writes nothing to the card; it reads
 * perf.txt once. It runs in a real disc/ISO game (sceKernelInitApitype, like the game plugin,
 * core/game.c) and, with the indicator set to Everywhere, in the XMB (ADR-059): there it only
 * shows the brightness bar - no numbers, no boost or steps, nothing before XMB_GRACE_MS, so it is
 * asleep through the boot's handoff to Home, where ADR-049's thread (then blocked in the display
 * driver, ADR-053) crashed Home's start. Home itself never loads a VSH line (ARK-4: VSH is an
 * apitype from 0x200; Home, its launcher, is a homebrew).
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

/* A framebuffer in VRAM (the game's), not the hub's kernel-partition page. Any cache mirror
   (0x04/0x44/0xA4) maps to the 2 MB at physical 0x04000000 (ADR-051). */
static int is_vram(const void *p)
{
    unsigned int a = (unsigned int)p & 0x1FFFFFFF;

    return a >= 0x04000000 && a < 0x04200000;
}

#define CFG               "ms0:/seplugins/pocketwatch/perf.txt"
#define HOLD_MS           1000 /* L + R + SELECT held this long: the next mode */
#define ELEC_S            10 /* the current: a command the syscon sends to the battery, so not often */
#define COMBO             (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_SELECT)
#define OTHERS            0x0000F0F8u /* the d-pad, the face buttons, START: none of them with the combo */
#define SCREEN_W          480
#define SCREEN_H          272
#define FRAME_US          15000 /* the draw loop's pace (~one frame); see perf_thread */
#define AV_EVERY          6 /* loops between readings of the volume and the brightness (~100 ms) */
#define GRACE_MS          2000 /* no drawing for the game's first seconds (its display is settling) */
#define XMB_GRACE_MS      5000 /* the XMB: asleep through the boot's handoff to Home (ADR-059) */
#define IMPOSE_VOLUME     0x1  /* pspimpose_driver.h */
#define IMPOSE_BRIGHTNESS 0x2
#define IMPOSE_MUTE       0x8

static volatile int g_run = 1;
static SceUID g_thread = -1;
static int g_xmb; /* in the XMB (ARK-4's VSH runlevel): the brightness bar only (ADR-059) */

static int g_mode = PW_PERF_FULL;
static pw_perf_cfg g_cfg;
static pw_perf_mask g_mask[PW_PERF_ROWS], g_osd_mask;
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
    if (g_xmb) { /* the XMB: the bar only; its brightness stays the system's (ADR-059) */
        g_mode = PW_PERF_OFF;
        g_cfg.boost = g_cfg.steps = 0;
    }
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
    char l[PW_PERF_ROWS][PW_PERF_COLS + 1];

    g_lines = pw_perf_rows(s, g_mode, l);
    for (int i = 0; i < g_lines; i++)
        pw_perf_render(l[i], &g_mask[i]);
}

/* The volume and the brightness (ADR-045, ADR-047): the indicator when one changed, the boost or
   Pocketwatch's steps kept. */
static pw_osd g_osd;
static pw_bright g_bright;
static pw_steps g_steps;
static pw_fbset g_fbs;   /* the game's display buffers: the overlay goes into all of them */
static int g_steps_set;  /* the first step taken from the level */
static int g_level = -1; /* the backlight's level as last read */
static int g_bright_n = PW_BRIGHT_STEPS; /* the brightness bar's steps (#11) */
static unsigned int g_t_screen;          /* SCREEN held last (a change of the owner's) */

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
    if (g_cfg.osd == PW_OSD_EVERY || (g_cfg.osd == PW_OSD_ALL && !g_xmb)) {
        /* the step of the backlight's level (#6, #11); the screen off (0) is no change of it */
        int bright = g_level > 0 ? pw_osd_bright(&g_cfg, g_level, &g_bright_n) : g_osd.bright;

        if (pw_osd_step(&g_osd, sceImposeGetParam(IMPOSE_VOLUME), sceImposeGetParam(IMPOSE_MUTE),
                        bright, now)) {
            char t[PW_PERF_COLS + 1];

            pw_osd_text(&g_osd, t);
            pw_perf_render(t, &g_osd_mask);
        }
    }
}

/* hub: the hub's frame is shown - it has its own volume bar; so has the XMB. The brightness is a
   vertical bar at the left (#7). */
static void osd_draw(void *top, int bufw, int fmt, unsigned int now, int hub)
{
    int kind = pw_osd_shown(&g_osd, now);

    if (kind == PW_OSD_VOLUME && !hub && !g_xmb)
        pw_osd_draw(top, bufw, fmt, SCREEN_W, SCREEN_H, &g_osd_mask,
                    g_osd.mute == 1 ? 0 : g_osd.vol, 30);
    else if (kind == PW_OSD_BRIGHT)
        pw_osd_draw_vbar(top, bufw, fmt, SCREEN_W, SCREEN_H, &g_osd_mask, g_osd.bright, g_bright_n);
}

/* A narrow card at the top left, a labelled row each (like Home's overlay): compact, not a wide
   banner (the owner, 2026-10-21). */
#define ROW_PITCH 9
static void lines_draw(void *top, int bufw, int fmt)
{
    int w = 0;

    for (int i = 0; i < g_lines; i++)
        w = g_mask[i].w > w ? g_mask[i].w : w;
    pw_perf_box(top, bufw, fmt, SCREEN_W, SCREEN_H, 3, 3, w + 6, g_lines * ROW_PITCH + 5);
    for (int i = 0; i < g_lines; i++)
        pw_perf_draw(top, bufw, fmt, SCREEN_W, SCREEN_H, 6, 5 + i * ROW_PITCH, &g_mask[i]);
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
    /* The XMB: asleep through the boot's handoff to Home - not even the file read before - and
       only in a delay, never in a driver, so module_stop ends it at once (ADR-053, ADR-059). */
    while (g_xmb && g_run && now_ms() - t0 < XMB_GRACE_MS)
        sceKernelDelayThread(50 * 1000);
    if (!g_run)
        return 0;
    read_mode();
    if (g_xmb && g_cfg.osd != PW_OSD_EVERY)
        return 0;
    pw_bright_init(&g_bright);
    find_idle();
    idle0 = idle_us();
    memset(&s, 0, sizeof(s));
    s.fps = -1;
    s.cpu_pct = s.free_kb = s.bat_pct = s.bat_ma = s.bat_mv = s.bat_temp = s.bat_min = PW_PERF_NA;
    lines(&s);
    while (g_run) {
        void *game = 0;
        int gw = 0, gf = 0;
        unsigned int now;
        SceCtrlData pad;

        /* Pace by the thread manager, not sceDisplayWaitVblankStart: a thread blocked in the
           display driver cannot be stopped while the game is quitting (the driver is torn down for
           the reboot to the XMB), so it outlived module_stop, ran in unloaded memory and the PSP
           black-screened and powered off (device, ADR-053). Every other Pocketwatch thread paces on
           sceKernelDelayThread for the same reason (plugin/src/core/main.c module_stop: "no thread
           of ours may outlive this function"). ~one frame; g_run is re-checked right after so a
           stop never reaches another display call. */
        sceKernelDelayThread(FRAME_US);
        if (!g_run)
            break;
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
                 (g_cfg.boost > 0 || g_cfg.steps || g_cfg.osd >= PW_OSD_ALL))
            av_step(now);
        /* L + R + SELECT held a second, nothing else: the next mode (once per hold; games only) */
        if (!g_xmb && (pad.Buttons & COMBO) == COMBO && !(pad.Buttons & OTHERS)) {
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
        if (!g_xmb && now - t_sec >= 1000) { /* (the numbers: games only) */
            unsigned long long wall = sceKernelGetSystemTimeWide(), idle = idle_us();

            s.fps = flips > 0 ? flips * 1000 / (int)(now - t_sec) : -1;
            /* a second's microseconds fit 32 bits: no 64-bit division (its helper alone was
               1.3 KB of the module's 16 KB) */
            unsigned int di = (unsigned int)(idle - idle0), dw = (unsigned int)(wall - wall0);

            s.cpu_pct = g_nidle && dw >= 100 ? 100 - (int)(di / (dw / 100)) : PW_PERF_NA;
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
        if (now - t0 < GRACE_MS) /* let the game set up its display first (the XMB: done above) */
            continue;
        if ((g_mode == PW_PERF_OFF || !g_lines) && pw_osd_shown(&g_osd, now) == PW_OSD_NONE)
            continue;
        /* Draw onto the game's own framebuffers, which live in VRAM (0x04000000-0x041FFFFF), and
           onto every one it shows (double/triple buffered) so the overlay never flickers. The hub's
           page is in a kernel partition, so it is never one of these: writing to a buffer we do not
           own faulted and hung the PSP (device: HOME in a game blurred then froze; ADR-051). The
           displayed buffer is deliberately NOT read to gate drawing -
           sceDisplayGetFrameBufferInternal did not return a usable address here and left the
           overlay invisible (device rounds 11a, 11c, ADR-053); while the hub is open the game's
           buffers are off-screen, so the overlay shows nothing but faults nothing. */
        if (game && gw >= SCREEN_W && is_vram(game))
            pw_fbset_add(&g_fbs, (unsigned int)game, gw, gf);
        for (int i = 0; i < g_fbs.n; i++) {
            void *t = (void *)(g_fbs.fb[i] | 0x40000000u); /* uncached: nothing to write back */

            if (g_mode != PW_PERF_OFF && g_lines)
                lines_draw(t, g_fbs.w[i], g_fbs.f[i]);
            osd_draw(t, g_fbs.w[i], g_fbs.f[i], now, 0);
        }
    }
    return 0;
}

int module_start(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    /* A real game, or the XMB (ARK-4's VSH runlevel: an apitype from 0x200, ADR-059); in Home
       (a homebrew) the thread never runs. In the XMB the thread ends at once unless the indicator
       is Everywhere (perf.txt). */
    {
        int a = sceKernelInitApitype();

        if (!is_disc_apitype(a) && a < 0x200)
            return 0;
        g_xmb = a >= 0x200;
    }
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
    if (g_thread >= 0) {
        /* No thread of ours may outlive this (the kernel unloads the module on the game's exit; a
           thread left in unloaded memory faults the reboot and the PSP powers off - ADR-053, and
           plugin/src/core/main.c). The draw loop paces on sceKernelDelayThread and re-checks g_run,
           so it ends within ~one frame; if it somehow does not, terminate it - safe now that it is
           never inside a display syscall. Same shape as the plugin's own threads. */
        SceUInt timeout = 120000;

        if (sceKernelWaitThreadEnd(g_thread, &timeout) < 0)
            sceKernelTerminateThread(g_thread);
        sceKernelDeleteThread(g_thread);
        g_thread = -1;
    }
    return 0;
}

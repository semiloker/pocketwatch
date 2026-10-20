// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Pocketwatch's performance overlay in games (PRF-REQ-001..005, ADR-043, docs/research/
 * m7-perf-overlay.md): a kernel module of its own, in ARK-4's PLUGINS.TXT only while Home's
 * Settings -> Overlay in Games is on - the game plugin's kernel RAM (2 KB left, ADR-037) pays
 * nothing, and a game without it runs as before. One thread: at every vblank it writes its lines
 * into the frame the PSP shows next (the hub's own while the hub is open, else the game's), once a
 * second it measures. It writes nothing to the card; it reads perf.txt once.
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

#define CFG      "ms0:/seplugins/pocketwatch/perf.txt"
#define HOLD_MS  1000 /* L + R + SELECT held this long: the next mode */
#define ELEC_S   10   /* the current: a command the syscon sends to the battery, so not often */
#define COMBO    (PSP_CTRL_LTRIGGER | PSP_CTRL_RTRIGGER | PSP_CTRL_SELECT)
#define OTHERS   0x0000F0F8u /* the d-pad, the face buttons, START: none of them with the combo */
#define SCREEN_W 480
#define SCREEN_H 272

static volatile int g_run = 1;
static SceUID g_thread = -1;

static int g_mode = PW_PERF_FULL;
static pw_perf_mask g_mask[2];
static int g_lines;

static SceUID g_idle[2]; /* the idle threads: what they ran is the CPU left over */
static int g_nidle;

static unsigned int now_ms(void)
{
    return sceKernelGetSystemTimeLow() / 1000;
}

static void read_mode(void)
{
    char t[32];
    SceUID fd = sceIoOpen(CFG, PSP_O_RDONLY, 0);
    int n = fd >= 0 ? sceIoRead(fd, t, sizeof(t)) : 0;

    if (fd >= 0)
        sceIoClose(fd);
    g_mode = pw_perf_parse(t, n > 0 ? n : 0);
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

static int perf_thread(SceSize args, void *argp)
{
    pw_perf_stats s;
    unsigned int t_sec = now_ms(), t_hold = 0, held = 0, elec_at = 0;
    unsigned long long idle0 = 0, wall0 = sceKernelGetSystemTimeWide();
    void *last = 0;
    int flips = 0;

    (void)args;
    (void)argp;
    read_mode();
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
        /* L + R + SELECT held a second, nothing else: the next mode (once per hold) */
        memset(&pad, 0, sizeof(pad));
        if (sceCtrlPeekBufferPositive(&pad, 1) > 0 && (pad.Buttons & COMBO) == COMBO &&
            !(pad.Buttons & OTHERS)) {
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
        if (g_mode == PW_PERF_OFF || !g_lines)
            continue;
        /* the hub's frame (priority 0) while it is open, else the game's */
        if (sceDisplayGetFrameBufferInternal(0, &top, &bufw, &fmt, PSP_DISPLAY_SETBUF_IMMEDIATE) <
                0 ||
            !top) {
            top = game;
            bufw = gw;
            fmt = gf;
        }
        if (!top || bufw < SCREEN_W)
            continue;
        top = (void *)((unsigned int)top | 0x40000000u); /* uncached: no cache to write back */
        for (int i = 0; i < g_lines; i++)
            pw_perf_draw(top, bufw, fmt, SCREEN_W, SCREEN_H, 4, 4 + i * 11, &g_mask[i]);
    }
    return 0;
}

int module_start(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
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

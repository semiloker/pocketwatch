// SPDX-License-Identifier: GPL-3.0-or-later
#include <malloc.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <psppower.h>
#include <psprtc.h>
#include <psputility_sysparam.h>
#include <string.h>

#include "gfx_gu.h"
#include "hw.h"
#include "stb_sprintf.h"

#include <pspwlan.h>

/* ARK-4 (src/stubs.S): find a kernel function, and run it for this user program. */
unsigned int sctrlHENFindFunction(const char *module, const char *library, unsigned int nid);
struct hm_kcall {
    unsigned int arg[12], ret1, ret2;
};
int kuKernelCall(void *func, struct hm_kcall *args);
int kuKernelGetModel(void);
int sctrlHENGetVersion(void);
int sctrlHENGetMinorVersion(void);

/* Pre-6.x NIDs, translated by sctrlHENFindFunction with ARK-4's table (the game plugin imports
   the same: plugin/src/core/stubs/sceImpose_driver.S; ARK-4's exitgame.c the peek). */
#define NID_SET_HOME_POPUP 0x5595A71A
#define NID_PEEK_DRIVER    0x3A622550

static volatile int g_exit;
static void *g_peek; /* sceCtrl_driver's peek: every button, HOME too */

static int exit_cb(int a, int b, void *c)
{
    (void)a;
    (void)b;
    (void)c;
    g_exit = 1;
    return 0;
}

static int callback_thread(SceSize args, void *argp)
{
    int cb = sceKernelCreateCallback("PocketwatchHomeExit", exit_cb, NULL);

    (void)args;
    (void)argp;
    sceKernelRegisterExitCallback(cb);
    sceKernelSleepThreadCB();
    return 0;
}

/* ---- the battery: a thread of its own ---- */

/* Out of the frame loop (ADR-031): with these calls in System's rows the PSP stopped ~300 ms
   about every second (home.log 2026-10-19); the cycle count is a command the syscon sends to the
   battery. Written into the buffer the screen does not read, then made the current one: the main
   thread (a higher priority) copies g_bat[g_bat_cur] and is never preempted by this one. */
static hm_battery g_bat[2];
static volatile int g_bat_cur = -1;
static char g_bat_cost[200];
static volatile int g_bat_costed;

static int read_cycles(void)
{
    void *get_cycle = /* the gauge's count, as the XMB plugin reads it (vsh/src/xbattery.c) */
        (void *)sctrlHENFindFunction("sceSYSCON_Driver", "sceSyscon_driver", 0xB5105D51);
    struct hm_kcall a;
    volatile int out = -1;

    memset(&a, 0, sizeof(a));
    a.arg[0] = (unsigned int)&out;
    return get_cycle && kuKernelCall(get_cycle, &a) >= 0 && (int)a.ret1 >= 0 ? out : -1;
}

static int battery_thread(SceSize args, void *argp)
{
    int cycles = -1;

    (void)args;
    (void)argp;
    for (int first = 1;; first = 0) {
        int n = g_bat_cur == 0; /* the other buffer */
        hm_battery *b = &g_bat[n];
        unsigned int t[8];

        t[0] = sceKernelGetSystemTimeLow();
        b->charging = scePowerIsBatteryCharging() == 1;
        b->pct = scePowerIsBatteryExist() == 1 ? scePowerGetBatteryLifePercent() : -1;
        if (b->pct < 0 || b->pct > 100)
            b->pct = -1;
        t[1] = sceKernelGetSystemTimeLow();
        b->left_min = scePowerGetBatteryLifeTime();
        t[2] = sceKernelGetSystemTimeLow();
        b->mv = scePowerGetBatteryVolt();
        t[3] = sceKernelGetSystemTimeLow();
        b->temp = scePowerGetBatteryTemp();
        t[4] = sceKernelGetSystemTimeLow();
        b->rcap = scePowerGetBatteryRemainCapacity();
        t[5] = sceKernelGetSystemTimeLow();
        b->full = scePowerGetBatteryFullCapacity();
        t[6] = sceKernelGetSystemTimeLow();
        if (first) /* (it moves by one in many charges: once) */
            cycles = read_cycles();
        t[7] = sceKernelGetSystemTimeLow();
        b->cycles = cycles;
        g_bat_cur = n;
        if (first) {
            stbsp_snprintf(g_bat_cost, sizeof(g_bat_cost),
                           "percent %u us, life time %u us, volt %u us, temp %u us, remaining %u "
                           "us, full %u us, cycles %u us",
                           t[1] - t[0], t[2] - t[1], t[3] - t[2], t[4] - t[3], t[5] - t[4],
                           t[6] - t[5], t[7] - t[6]);
            g_bat_costed = 1;
        }
        sceKernelDelayThread(1000 * 1000);
    }
    return 0;
}

int hm_hw_battery(hm_battery *b)
{
    int cur = g_bat_cur;

    if (cur < 0) {
        memset(b, 0xFF, sizeof(*b)); /* every field -1 */
        b->charging = 0;
        return -1;
    }
    *b = g_bat[cur];
    return 0;
}

int hm_hw_battery_cost(char *out, int cap)
{
    if (!g_bat_costed)
        return 0;
    stbsp_snprintf(out, cap, "%s", g_bat_cost);
    return 1;
}

int hm_hw_init(void)
{
    SceUID th = sceKernelCreateThread("PocketwatchHomeCb", callback_thread, 0x11, 0x1000, 0, 0);
    void *popup;

    if (th >= 0)
        sceKernelStartThread(th, 0, 0);
    /* below the main thread (0x20), above the loader (0x30) */
    th = sceKernelCreateThread("PocketwatchHomeBattery", battery_thread, 0x28, 0x4000, 0, 0);
    if (th >= 0)
        sceKernelStartThread(th, 0, 0);
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    /* HOME is ours (HOM-REQ-010): the system's quit dialog off, the button read in kernel mode.
       Only when both are found - else HOME keeps the system's dialog. */
    g_peek =
        (void *)sctrlHENFindFunction("sceController_Service", "sceCtrl_driver", NID_PEEK_DRIVER);
    popup =
        (void *)sctrlHENFindFunction("sceImpose_Driver", "sceImpose_driver", NID_SET_HOME_POPUP);
    if (g_peek && popup) {
        struct hm_kcall a;

        memset(&a, 0, sizeof(a));
        if (kuKernelCall(popup, &a) < 0)
            g_peek = 0;
    } else {
        g_peek = 0;
    }
    return hm_gu_init();
}

int hm_hw_home_ours(void)
{
    return g_peek != 0;
}

unsigned int hm_hw_buttons(void)
{
    SceCtrlData pad;

    if (g_peek) {
        struct hm_kcall a;

        memset(&a, 0, sizeof(a));
        memset(&pad, 0, sizeof(pad));
        a.arg[0] = (unsigned int)&pad;
        a.arg[1] = 1;
        if (kuKernelCall(g_peek, &a) >= 0 && (int)a.ret1 > 0)
            return pad.Buttons;
    }
    return sceCtrlPeekBufferPositive(&pad, 1) < 0 ? 0 : pad.Buttons;
}

unsigned int hm_hw_now_ms(void)
{
    return sceKernelGetSystemTimeLow() / 1000;
}

int hm_hw_exit_requested(void)
{
    return g_exit;
}

/* ---- the system's parameters: kept ---- */

#define PARAMS 16
static int g_param[PARAMS];
static unsigned int g_param_read; /* a bit per id */

int hm_hw_param(int id)
{
    int v = 0;

    if (id < 0 || id >= PARAMS)
        return sceUtilityGetSystemParamInt(id, &v) == 0 ? v : -1;
    if (!(g_param_read & 1u << id)) {
        g_param[id] = sceUtilityGetSystemParamInt(id, &v) == 0 ? v : -1;
        g_param_read |= 1u << id;
    }
    return g_param[id];
}

int hm_hw_set_param(int id, int v)
{
    int r = sceUtilitySetSystemParamInt(id, v);

    if (id >= 0 && id < PARAMS)
        g_param_read &= ~(1u << id); /* read back */
    return r == 0 && hm_hw_param(id) == v ? 0 : -1;
}

static void format(const ScePspDateTime *t, int year, char *out, int cap)
{
    int datef = hm_hw_param(PSP_SYSTEMPARAM_ID_INT_DATE_FORMAT),
        timef = hm_hw_param(PSP_SYSTEMPARAM_ID_INT_TIME_FORMAT);
    char tm[16], dt[16];

    if (timef == PSP_SYSTEMPARAM_TIME_FORMAT_12HR)
        stbsp_snprintf(tm, sizeof(tm), "%d:%02d %s", t->hour % 12 ? t->hour % 12 : 12, t->minute,
                       t->hour < 12 ? "AM" : "PM");
    else
        stbsp_snprintf(tm, sizeof(tm), "%d:%02d", t->hour, t->minute);
    if (datef == PSP_SYSTEMPARAM_DATE_FORMAT_DDMMYYYY)
        stbsp_snprintf(dt, sizeof(dt), year ? "%d/%d/%d" : "%d/%d", t->day, t->month, t->year);
    else if (datef == PSP_SYSTEMPARAM_DATE_FORMAT_YYYYMMDD && year)
        stbsp_snprintf(dt, sizeof(dt), "%d/%d/%d", t->year, t->month, t->day);
    else
        stbsp_snprintf(dt, sizeof(dt), year ? "%d/%d/%d" : "%d/%d", t->month, t->day, t->year);
    stbsp_snprintf(out, cap, "%s %s", dt, tm);
}

void hm_hw_clock(char *out, int cap)
{
    ScePspDateTime t;

    if (sceRtcGetCurrentClockLocalTime(&t) < 0) {
        stbsp_snprintf(out, cap, "--:--");
        return;
    }
    format(&t, 0, out, cap);
}

void hm_hw_format_time(const ScePspDateTime *t, char *out, int cap)
{
    format(t, 1, out, cap);
}

int hm_hw_cross_enter(void)
{
    return hm_hw_param(PSP_SYSTEMPARAM_ID_INT_UNKNOWN) != 0; /* 9: the confirm button (else X) */
}

int hm_hw_language(void)
{
    int lang = hm_hw_param(PSP_SYSTEMPARAM_ID_INT_LANGUAGE);

    return lang < 0 ? PSP_SYSTEMPARAM_LANGUAGE_ENGLISH : lang;
}

unsigned int hm_hw_free_mb(void)
{
    /* sceIoDevctl 0x02425818 fills a pointer to: max clusters, free clusters, max sectors,
       sector size, sectors per cluster (PSPSDK samples, used by every file manager) */
    unsigned int info[5] = {0, 0, 0, 0, 0}, *p = info;

    if (sceIoDevctl("ms0:", 0x02425818, &p, sizeof(p), NULL, 0) < 0)
        return 0;
    return (unsigned int)((unsigned long long)info[1] * info[3] * info[4] / (1024 * 1024));
}

int hm_hw_read_file(const char *path, void *buf, unsigned int cap)
{
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    int n;

    if (fd < 0)
        return fd;
    n = sceIoRead(fd, buf, cap);
    sceIoClose(fd);
    return n;
}

void hm_hw_touch(const char *path)
{
    SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    u64 now = 0;
    char text[24];

    if (fd >= 0) {
        sceRtcGetCurrentTick(&now);
        stbsp_snprintf(text, sizeof(text), "%llu", (unsigned long long)now);
        sceIoWrite(fd, text, strlen(text));
        sceIoClose(fd);
    }
}

void hm_hw_remove(const char *path)
{
    sceIoRemove(path);
}

void hm_hw_model(char *out, int cap)
{
    /* kuKernelGetModel: 0 PSP-1000, 1 PSP-2000, 2/3/6/8 PSP-3000 boards, 4 PSP Go, 10 E1000 */
    static const char *const names[11] = {"PSP-1000", "PSP-2000", "PSP-3000",  "PSP-3000",
                                          "PSP Go",   "PSP",      "PSP-3000",  "PSP-3000",
                                          "PSP-3000", "PSP-3000", "PSP Street"};
    int m = kuKernelGetModel();

    stbsp_snprintf(out, cap, "%s", m >= 0 && m < 11 ? names[m] : "PSP");
}

void hm_hw_firmware(char *out, int cap)
{
    unsigned int v = (unsigned int)sceKernelDevkitVersion(); /* 0x06060110: 6.61 */

    stbsp_snprintf(out, cap, "%u.%u%u", v >> 24, (v >> 16) & 0xFF, (v >> 8) & 0xFF);
}

void hm_hw_cfw(char *out, int cap)
{
    int minor = sctrlHENGetVersion(), micro = sctrlHENGetMinorVersion();

    if (minor < 0 || minor > 999 || micro < 0 || micro > 999) /* not ARK-4's answer (an error) */
        stbsp_snprintf(out, cap, "ARK-4");
    else
        stbsp_snprintf(out, cap, "ARK-4 4.%d.%d", minor, micro);
}

void hm_hw_mac(char *out, int cap)
{
    unsigned char m[8];

    if (sceWlanGetEtherAddr(m) < 0) {
        out[0] = 0;
        return;
    }
    stbsp_snprintf(out, cap, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}

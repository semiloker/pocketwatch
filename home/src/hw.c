// SPDX-License-Identifier: GPL-3.0-or-later
#include <malloc.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <psppower.h>
#include <pspreg.h>
#include <psprtc.h>
#include <psputility_sysparam.h>
#include <string.h>

#include "battcal.h"
#include "clockfix.h"
#include "core/battmodel.h"
#include "gfx_gu.h"
#include "hcfg.h"
#include "hw.h"
#include "stb_sprintf.h"
#include "ui/i18n.h"

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

/* The current the battery's gauge measures (mA, negative while discharging; VERIFIED 2026-10-17),
   as the XMB plugin reads it (vsh/src/xbattery.c); PW_BATT_UNKNOWN when it cannot. */
static int read_elec(void)
{
    void *get_elec =
        (void *)sctrlHENFindFunction("sceSYSCON_Driver", "sceSyscon_driver", 0x373EC933);
    struct hm_kcall a;
    volatile int out = 0;

    memset(&a, 0, sizeof(a));
    a.arg[0] = (unsigned int)&out;
    return get_elec && kuKernelCall(get_elec, &a) >= 0 && (int)a.ret1 >= 0 ? out : PW_BATT_UNKNOWN;
}

/* Unix seconds now, 0 while the clock is lost. */
static unsigned long long unix_now(void)
{
    u64 tick;

    return sceRtcGetCurrentTick(&tick) < 0 ? 0 : hm_unix_us_of_tick(tick) / HM_US_PER_S;
}

/* While USB lends the card to the PC nothing of Home's may write to it (the PC's FAT and ours
   would not agree): the other threads' writes wait or are dropped meanwhile (hm_hw_card_lent). */
static volatile int g_card_lent;

static void append(const char *path, const char *text, int truncate)
{
    SceUID fd =
        g_card_lent
            ? -1
            : sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | (truncate ? PSP_O_TRUNC : PSP_O_APPEND),
                        0777);

    if (fd < 0)
        return;
    sceIoWrite(fd, text, strlen(text));
    sceIoClose(fd);
}

/* ---- the calibration run (BAT-REQ-008..010, ADR-042): on this thread, so the samples are
   written whatever the screen does; the run's file survives the PSP turning itself off ---- */

#define CAL_RUN     "ms0:/seplugins/pocketwatch/battery-cal.txt"
#define CAL_RESULT  "ms0:/seplugins/pocketwatch/battery.txt"
#define CAL_EVERY_S 30
#define CAL_TEXT    (96 * 1024) /* a run of 12 h at 30 s: 1440 lines of ~35 bytes */

static volatile int g_cal_cmd; /* from the main thread: 1 start, 2 stop */
static volatile int g_cal = HM_CAL_OFF;
static volatile int g_cal_news; /* HM_CAL_NEWS_* once, for a toast */
static hm_battcal g_cal_res;    /* the last result (battery.txt) */
static volatile int g_cal_have;
static unsigned int g_cal_last; /* the last sample (ms) */

static int read_run(hm_battcal *c)
{
    char *text = malloc(CAL_TEXT);
    int n = text ? hm_hw_read_file(CAL_RUN, text, CAL_TEXT) : -1,
        r = n > 0 ? hm_battcal_parse(text, (unsigned int)n, c) : -1;

    free(text);
    return r;
}

/* The run ends (the charger came back, Stop, or the PSP turned off during it): its result kept
   when it covered enough of the charge. */
static void cal_finish(void)
{
    hm_battcal c;
    char line[96];

    if (read_run(&c) == 0 && c.est_full_mah > 0) {
        hm_battcal_result_line(line, sizeof(line), &c);
        append(CAL_RESULT, line, 1);
        g_cal_res = c;
        g_cal_have = 1;
        g_cal_news = HM_CAL_NEWS_DONE;
        stbsp_snprintf(line, sizeof(line), "home battery: measured %d mAh in %u s (%d%% to %d%%)\n",
                       c.est_full_mah, c.seconds, c.pct_from, c.pct_to);
        hm_hw_log(line);
    } else {
        g_cal_news = HM_CAL_NEWS_SHORT;
    }
    sceIoRemove(CAL_RUN);
    g_cal = HM_CAL_OFF;
}

static void cal_sample(const hm_battery *b, int start)
{
    char line[96];
    unsigned long long t = unix_now();

    if (start) {
        hm_battcal_start_line(line, sizeof(line), t, b->full, b->cycles);
        append(CAL_RUN, line, 1);
    }
    hm_battcal_sample_line(line, sizeof(line), t, b->pct, b->mv, b->ma, b->rcap, b->online);
    append(CAL_RUN, line, 0);
    g_cal_last = sceKernelGetSystemTimeLow() / 1000;
}

/* Charge to full, unplug, run down: one step a second. */
static void cal_step(const hm_battery *b)
{
    int cmd = g_cal_cmd, full = b->pct >= 98 && !b->charging, begin = 0;

    if (g_card_lent) /* (USB: the card is the PC's; the run waits, its end too) */
        return;
    g_cal_cmd = 0;
    if (cmd == 2) {
        if (g_cal == HM_CAL_RUN)
            cal_finish();
        g_cal = HM_CAL_OFF;
        return;
    }
    if (cmd == 1 && g_cal == HM_CAL_OFF)
        g_cal = HM_CAL_CHARGE;
    if (g_cal == HM_CAL_CHARGE && full) {
        if (b->online)
            g_cal = HM_CAL_UNPLUG;
        else
            begin = 1; /* (full and unplugged already: at once) */
    } else if (g_cal == HM_CAL_UNPLUG && !b->online) {
        begin = 1;
    }
    if (begin) {
        g_cal = HM_CAL_RUN;
        cal_sample(b, 1);
        hm_hw_log("home battery: calibration run started\n");
    } else if (g_cal == HM_CAL_RUN) {
        if (b->online) { /* the charger again: the run is over */
            cal_finish();
            return;
        }
        if (sceKernelGetSystemTimeLow() / 1000 - g_cal_last >= CAL_EVERY_S * 1000)
            cal_sample(b, 0);
        scePowerTick(PSP_POWER_TICK_SUSPEND); /* no automatic sleep in the middle of it */
    }
}

/* At the start: the last result, and a run the PSP turned off in (its end) or that Home left. */
static void cal_resume(const hm_battery *b)
{
    char text[128];
    int n = hm_hw_read_file(CAL_RESULT, text, sizeof(text) - 1);
    hm_battcal c;

    if (n > 0 && hm_battcal_result_parse(text, (unsigned int)n, &g_cal_res) == 0)
        g_cal_have = 1;
    if (read_run(&c) < 0) {
        sceIoRemove(CAL_RUN); /* (nothing usable in it) */
        return;
    }
    if (c.pct_to > 10 && !b->online) { /* Home was left in the middle: go on */
        g_cal = HM_CAL_RUN;
        g_cal_last = 0;
    } else {
        cal_finish();
    }
}

void hm_hw_cal(int start)
{
    g_cal_cmd = start ? 1 : 2;
}

int hm_hw_cal_result(hm_battcal *out)
{
    if (!g_cal_have)
        return -1;
    *out = g_cal_res;
    return 0;
}

int hm_hw_cal_news(void)
{
    int n = g_cal_news;

    g_cal_news = 0;
    return n;
}

/* Our estimate (core/battmodel.c, BAT-REQ-003): a reading every 15 s into its history. */
static pw_batt_history g_hist;

static void estimate(hm_battery *b, unsigned int now_ms)
{
    pw_batt_sample s;
    pw_batt_estimate e;

    memset(&s, 0, sizeof(s));
    s.t_ms = now_ms;
    s.percent = b->pct;
    s.mv = b->mv;
    s.ma = b->ma;
    s.temp_c = b->temp;
    s.rcap_mah = b->rcap;
    s.full_mah = b->full;
    s.cycles = b->cycles;
    s.fw_minutes = b->left_min;
    s.present = b->pct >= 0;
    s.charging = b->charging;
    s.online = b->online;
    pw_batt_validate(&s);
    pw_batt_history_add(&g_hist, &s);
    pw_batt_estimate_now(&g_hist, &e);
    b->est_state = e.state;
    b->est_min = e.minutes;
    b->est_conf = e.confidence;
    b->avg_ma = e.avg_ma;
}

static int battery_thread(SceSize args, void *argp)
{
    int cycles = -1, ma = PW_BATT_UNKNOWN, est_state = PW_BATT_STATE_UNKNOWN, est_min = -1,
        est_conf = PW_BATT_EST_NONE, avg_ma = PW_BATT_UNKNOWN;

    (void)args;
    (void)argp;
    pw_batt_history_reset(&g_hist);
    for (int first = 1, tick = 0;; first = 0, tick++) {
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
        b->online = scePowerIsPowerOnline() == 1;
        /* the current: a command the syscon sends to the battery (~35 ms, like the cycles), so
           every 10 s - not every second */
        if (tick % 10 == 0 && b->pct >= 0)
            ma = read_elec();
        b->ma = ma;
        b->est_state = est_state;
        b->est_min = est_min;
        b->est_conf = est_conf;
        b->avg_ma = avg_ma;
        if (tick % 15 == 0) {
            estimate(b, t[0] / 1000);
            est_state = b->est_state;
            est_min = b->est_min;
            est_conf = b->est_conf;
            avg_ma = b->avg_ma;
        }
        if (first)
            cal_resume(b);
        cal_step(b);
        b->cal = g_cal;
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
        b->charging = b->online = 0;
        b->ma = b->avg_ma = PW_BATT_UNKNOWN;
        b->cal = HM_CAL_OFF;
        return -1;
    }
    *b = g_bat[cur];
    return 0;
}

/* ---- the clock (HOM-REQ-031, ADR-041) ---- */

#define DIR "ms0:/seplugins/pocketwatch/"

/* sceRtcSetCurrentTick(u64 *tick), UTC: sceRtc_driver's pre-6.x NID, translated by ARK-4's table
   (6.60: 0x929620CE, uOFW src/kd/rtc/rtc.c); what NTPSP's kernel helper calls (joel16/NTPSP,
   plugin/rtc_driver.c). Through KUBridge, as Home reads the syscon. */
#define NID_RTC_SET_CURRENT_TICK 0x9763C138

int hm_hw_set_utc(unsigned long long unix_us)
{
    void *set =
        (void *)sctrlHENFindFunction("sceRTC_Service", "sceRtc_driver", NID_RTC_SET_CURRENT_TICK);
    u64 tick = hm_tick_of_unix_us(unix_us), back = 0;
    struct hm_kcall a;
    char line[120];
    int r;

    if (!set)
        return -1;
    memset(&a, 0, sizeof(a));
    a.arg[0] = (unsigned int)&tick;
    r = kuKernelCall(set, &a) < 0 ? -1 : (int)a.ret1;
    /* read back: the clock is what was asked for (within a few seconds) */
    if (sceRtcGetCurrentTick(&back) < 0)
        back = 0;
    stbsp_snprintf(line, sizeof(line), "home clock: set to %llu s: 0x%08X, reads %llu s\n",
                   unix_us / HM_US_PER_S, (unsigned int)r, hm_unix_us_of_tick(back) / HM_US_PER_S);
    hm_hw_log(line);
    return r >= 0 && back + 5 * HM_US_PER_S >= tick && back <= tick + 5 * HM_US_PER_S ? 0 : -1;
}

/* The files whose time says when the PSP last knew it: what Pocketwatch writes (the clock at its
   last write: the XMB plugin's at every start, the game plugin's in every game, Home's every 30 s)
   and what the PC copied (its own clock: scripts/deploy.ps1 keeps the build's time). */
static const char *const k_clock_files[] = {DIR "home.log",
                                            DIR "pocketwatch.log",
                                            DIR "xmb.log",
                                            DIR "playtime-a.txt",
                                            DIR "playtime-b.txt",
                                            DIR "home.txt",
                                            DIR "config.txt",
                                            DIR "boot.txt",
                                            DIR "favorites.txt",
                                            DIR "pocketwatch.prx",
                                            DIR "pocketwatch_xmb.prx",
                                            DIR "pocketwatch_perf.prx",
                                            "ms0:/PSP/GAME/PocketwatchHome/EBOOT.PBP",
                                            "ms0:/PSP/SAVEDATA/ARK_01234/PWHOME.PBP"};
#define CLOCK_FILES ((int)(sizeof(k_clock_files) / sizeof(k_clock_files[0])))

int hm_hw_clock_check(void)
{
    unsigned long long seen[CLOCK_FILES], newest;
    u64 tick = 0;
    int rtc = sceRtcGetCurrentTick(&tick), n = 0;
    char line[160];

    if (!hm_clock_lost(rtc, hm_unix_us_of_tick(tick) / HM_US_PER_S))
        return HM_CLOCK_OK;
    for (int i = 0; i < CLOCK_FILES; i++) {
        SceIoStat st;
        u64 local, utc;

        if (sceIoGetstat(k_clock_files[i], &st) < 0 || sceRtcGetTick(&st.sce_st_mtime, &local) < 0)
            continue;
        sceRtcConvertLocalTimeToUTC(&local, &utc); /* (FAT keeps the local time) */
        seen[n++] = hm_unix_us_of_tick(utc) / HM_US_PER_S;
    }
    newest = hm_clock_newest(seen, n);
    stbsp_snprintf(line, sizeof(line),
                   "home clock: lost (0x%08X, %llu s); the card's newest time %llu s of %d files\n",
                   (unsigned int)rtc, hm_unix_us_of_tick(tick) / HM_US_PER_S, newest, n);
    hm_hw_log(line);
    if (newest && hm_hw_set_utc(newest * HM_US_PER_S) == 0)
        return HM_CLOCK_RESTORED;
    return HM_CLOCK_LOST;
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

void hm_hw_clock(char *out, int cap, int style)
{
    ScePspDateTime t;
    char s[40];
    int wd;

    if (sceRtcGetCurrentClockLocalTime(&t) < 0) {
        stbsp_snprintf(out, cap, "--:--");
        return;
    }
    format(&t, 0, s, sizeof(s));
    if (style == HM_CLOCK_DATE) {
        stbsp_snprintf(out, cap, "%s", s);
        return;
    }
    { /* the time alone: what follows the date */
        const char *sp = strchr(s, ' ');

        if (style == HM_CLOCK_TIME) {
            stbsp_snprintf(out, cap, "%s", sp ? sp + 1 : s);
            return;
        }
        wd = sceRtcGetDayOfWeek(t.year, t.month, t.day);
        stbsp_snprintf(out, cap, "%s %s", pw_tr(PW_S_WD_0 + (wd >= 0 && wd < 7 ? wd : 0)),
                       sp ? sp + 1 : s);
    }
}

/* sceDisplay_driver's brightness, pre-6.x NIDs (translated by ARK-4's table): 0 turns the
   backlight off, as the HOLD plugins do. */
#define NID_DISPLAY_SET_BRIGHTNESS 0x9E3C6DC6
#define NID_DISPLAY_GET_BRIGHTNESS 0x31C4BAA8

int hm_hw_backlight(int on)
{
    static int level = -1, extra;
    void *set = (void *)sctrlHENFindFunction("sceDisplay_Service", "sceDisplay_driver",
                                             NID_DISPLAY_SET_BRIGHTNESS),
         *get = (void *)sctrlHENFindFunction("sceDisplay_Service", "sceDisplay_driver",
                                             NID_DISPLAY_GET_BRIGHTNESS);
    struct hm_kcall a;

    if (!set)
        return -1;
    memset(&a, 0, sizeof(a));
    if (!on) {
        int l = 0, e = 0;

        if (get) { /* what to give back */
            a.arg[0] = (unsigned int)&l;
            a.arg[1] = (unsigned int)&e;
            if (kuKernelCall(get, &a) >= 0 && l > 0) {
                level = l;
                extra = e;
            }
            memset(&a, 0, sizeof(a));
        }
        a.arg[0] = 0;
        a.arg[1] = 0;
    } else {
        a.arg[0] = (unsigned int)(level > 0 ? level : 72);
        a.arg[1] = (unsigned int)extra;
    }
    return kuKernelCall(set, &a) < 0 ? -1 : 0;
}

void hm_hw_format_time(const ScePspDateTime *t, char *out, int cap)
{
    format(t, 1, out, cap);
}

/* The XMB's theme colour (registry /CONFIG/SYSTEM/XMB/THEME color_mode: 0 the month's sky, 1-12
   one of the twelve), read through KUBridge as the game plugin reads it in the kernel
   (plugin/src/core/sysinfo.c); sceReg_driver's pre-6.x NIDs, translated by ARK-4's table. */
#define NID_REG_OPEN      0x92E41280
#define NID_REG_CLOSE     0xFA8A5739
#define NID_REG_OPEN_CAT  0x1D8A762E
#define NID_REG_CLOSE_CAT 0x0CAE832B
#define NID_REG_KEY_INFO  0xD4475AA8
#define NID_REG_KEY_VALUE 0x28A8E98A

static int reg_call(unsigned int nid, unsigned int a0, unsigned int a1, unsigned int a2,
                    unsigned int a3, unsigned int a4)
{
    void *f = (void *)sctrlHENFindFunction("sceRegistry_Service", "sceReg_driver", nid);
    struct hm_kcall a;

    if (!f)
        return -1;
    memset(&a, 0, sizeof(a));
    a.arg[0] = a0;
    a.arg[1] = a1;
    a.arg[2] = a2;
    a.arg[3] = a3;
    a.arg[4] = a4;
    return kuKernelCall(f, &a) < 0 ? -1 : (int)a.ret1;
}

static int theme_colour(void)
{
    struct RegParam param;
    REGHANDLE reg = 0, cat = 0, key = 0;
    unsigned int type = 0, value = 0;
    SceSize size = 0;
    int colour = -1;

    memset(&param, 0, sizeof(param));
    param.regtype = 1;
    strcpy(param.name, "/system");
    param.namelen = strlen(param.name);
    param.unk2 = 1;
    param.unk3 = 1;
    if (reg_call(NID_REG_OPEN, (unsigned int)&param, 2, (unsigned int)&reg, 0, 0) < 0) /* 2: read */
        return -1;
    if (reg_call(NID_REG_OPEN_CAT, reg, (unsigned int)"/CONFIG/SYSTEM/XMB/THEME", 2,
                 (unsigned int)&cat, 0) >= 0) {
        if (reg_call(NID_REG_KEY_INFO, cat, (unsigned int)"color_mode", (unsigned int)&key,
                     (unsigned int)&type, (unsigned int)&size) >= 0 &&
            size == 4 && reg_call(NID_REG_KEY_VALUE, cat, key, (unsigned int)&value, 4, 0) >= 0)
            colour = (int)value;
        reg_call(NID_REG_CLOSE_CAT, cat, 0, 0, 0, 0);
    }
    reg_call(NID_REG_CLOSE, reg, 0, 0, 0, 0);
    return colour;
}

int hm_hw_sky_index(int *colour)
{
    ScePspDateTime t;

    *colour = theme_colour();
    if (*colour >= 1 && *colour <= 12)
        return *colour - 1;
    return sceRtcGetCurrentClockLocalTime(&t) >= 0 && t.month >= 1 && t.month <= 12 ? t.month - 1
                                                                                    : 0;
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

void hm_hw_card_lent(int lent)
{
    g_card_lent = lent;
}

void hm_hw_log(const char *line)
{
    SceUID fd = g_card_lent ? -1
                            : sceIoOpen("ms0:/seplugins/pocketwatch/home.log",
                                        PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);

    if (fd >= 0) {
        sceIoWrite(fd, line, strlen(line));
        sceIoClose(fd);
    }
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

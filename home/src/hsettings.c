// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspkernel.h>
#include <psputility_sysparam.h>
#include <string.h>

#include "core/config.h"
#include "core/platform.h"
#include "hcfg.h"
#include "hm_icons.h"
#include "home_view.h"
#include "hsettings.h"
#include "hw.h"
#include "overlay.h"
#include "plugins_txt.h"
#include "sound.h"
#include "stb_sprintf.h"
#include "ui/i18n.h"
#include "ui/langnames.h"

#define DIR      "ms0:/seplugins/pocketwatch/"
#define HOME_CFG DIR "home.txt"

/* ARK-4 (src/stubs.S), as hw.c. */
unsigned int sctrlHENFindFunction(const char *module, const char *library, unsigned int nid);
struct hs_kcall {
    unsigned int arg[12], ret1, ret2;
};
int kuKernelCall(void *func, struct hs_kcall *args);

/* sceImpose_driver, pre-6.x NIDs (ARK-4's table translates them; tools/nids/ark4_660.tsv). */
#define NID_IMPOSE_GET    0x531C9778
#define NID_IMPOSE_SET    0x810FB7FB
#define IMPOSE_BRIGHTNESS 0x2  /* pspimpose_driver.h: 0-3 (PPSSPP, JPCSP: 3 by default) */
#define IMPOSE_AVLS       0x10 /* 0 or 1 */

static int impose(int set, int param, int value)
{
    static void *get_fn, *set_fn;
    static int looked;
    struct hs_kcall a;
    void *f;

    if (!looked) {
        looked = 1;
        get_fn =
            (void *)sctrlHENFindFunction("sceImpose_Driver", "sceImpose_driver", NID_IMPOSE_GET);
        set_fn =
            (void *)sctrlHENFindFunction("sceImpose_Driver", "sceImpose_driver", NID_IMPOSE_SET);
    }
    f = set ? set_fn : get_fn;
    if (!f)
        return -1;
    memset(&a, 0, sizeof(a));
    a.arg[0] = (unsigned int)param;
    a.arg[1] = (unsigned int)value;
    if (kuKernelCall(f, &a) < 0)
        return -1;
    return (int)a.ret1;
}

/* ---- the rows ---- */

typedef struct {
    int cat; /* HS_CAT_* */
    int kind, label, action;
    int (*count)(void);
    int (*get)(void);       /* the current option, -1 when unknown */
    int (*set)(int option); /* < 0 refused, else 0 or HS_TEXTS_CHANGED */
    void (*text)(int option, char *out, int cap);
} row;

static int two(void)
{
    return 2;
}

static void on_off(int o, char *out, int cap)
{
    stbsp_snprintf(out, cap, "%s", pw_tr(o ? PW_S_ON : PW_S_OFF));
}

/* Pocketwatch: config.txt */
static int save_config(void)
{
    return pw_config_save(DIR "config.txt") < 0 ? -1 : 0;
}

static int get_start(void)
{
    return pw_config_get()->start == 1;
}
static int set_start(int o)
{
    pw_config_get()->start = o;
    return save_config();
}
static void text_start(int o, char *out, int cap)
{
    stbsp_snprintf(out, cap, "%s", pw_tr(o ? PW_S_START_HOME : PW_S_START_XMB));
}

static int three(void)
{
    return 3;
}
static int get_style(void)
{
    return pw_config_get()->style;
}
static int set_style(int o)
{
    pw_config_get()->style = o;
    return save_config() < 0 ? -1 : HS_LOOK_CHANGED;
}
static void text_style(int o, char *out, int cap)
{
    static const int ids[3] = {PW_S_STYLE_XMB, PW_S_STYLE_MODERN, PW_S_STYLE_MINIMAL};

    stbsp_snprintf(out, cap, "%s", pw_tr(ids[o % 3]));
}

static int get_sort(void)
{
    return pw_config_get()->sort == 1;
}
static int set_sort(int o)
{
    pw_config_get()->sort = o;
    return save_config();
}
static void text_sort(int o, char *out, int cap)
{
    stbsp_snprintf(out, cap, "%s", pw_tr(o ? PW_S_HS_SORT_RECENT : PW_S_HS_SORT_NAME));
}

/* Home's wallpaper: wallpaper.bin holds it as Home draws it (512 x 272, made once when it was
   chosen in Photos with Square), wallpaper.txt the picture's path for its name here. Without
   them, Pocketwatch's own background. The name is kept here: the rows are built every second,
   and the card is not read while USB has it. */
#define WALLPAPER     DIR "wallpaper.txt"
#define WALLPAPER_BIN DIR "wallpaper.bin"
#define WALLPAPER_LEN (512u * 272u * 4u)
static char g_wp_name[64];
static int g_wp_known; /* 0: not read yet, 1: none, 2: g_wp_name */
static int get_wallpaper(void)
{
    if (!g_wp_known) {
        char path[200];
        int n = pw_file_read(WALLPAPER, path, sizeof(path) - 1);
        const char *slash;

        g_wp_known = 1;
        if (n > 0) {
            path[n] = 0;
            slash = strrchr(path, '/');
            stbsp_snprintf(g_wp_name, sizeof(g_wp_name), "%s", slash ? slash + 1 : path);
            g_wp_known = 2;
        }
    }
    return g_wp_known == 2;
}
int hs_wallpaper_load(unsigned int *out)
{
    return pw_file_read(WALLPAPER_BIN, out, WALLPAPER_LEN) == (int)WALLPAPER_LEN ? 0 : -1;
}

int hs_wallpaper_set(const char *path, const unsigned int *bg)
{
    if (pw_file_write(WALLPAPER_BIN, bg, WALLPAPER_LEN, 0) < 0 ||
        pw_file_write(WALLPAPER, path, (unsigned int)strlen(path), 0) < 0) {
        pw_file_remove(WALLPAPER_BIN);
        return -1;
    }
    g_wp_known = 0;
    hm_cfg_get()->bg = HM_BG_WALLPAPER; /* what was chosen is shown */
    hm_cfg_save(HOME_CFG);
    return 0;
}

/* ---- Home's own (home.txt, hcfg.h): Appearance and Music ---- */

static unsigned int g_sky_colour[12]; /* the months' colours, for the swatches (hs_set_skies) */

void hs_set_skies(const unsigned int colours[12])
{
    memcpy(g_sky_colour, colours, sizeof(g_sky_colour));
}

static int save_home(int r)
{
    return hm_cfg_save(HOME_CFG) < 0 ? -1 : r;
}

static int theme_count(void)
{
    return HM_THEMES;
}
static int get_theme(void)
{
    return hm_cfg_get()->theme;
}
static int set_theme(int o)
{
    hm_cfg_get()->theme = o;
    return save_home(HS_LOOK_CHANGED);
}
static void text_theme(int o, char *out, int cap)
{
    static const int k[HM_THEMES] = {PW_S_HS_THEME_DARK, PW_S_HS_THEME_GLASS, PW_S_HS_THEME_OLED,
                                     PW_S_HS_THEME_PAPER};

    stbsp_snprintf(out, cap, "%s", pw_tr(k[o >= 0 && o < HM_THEMES ? o : 0]));
}

static int accent_count(void)
{
    return HM_ACCENTS;
}
static int get_accent(void)
{
    return hm_cfg_get()->accent;
}
static int set_accent(int o)
{
    hm_cfg_get()->accent = o;
    return save_home(HS_LOOK_CHANGED);
}
static void text_accent(int o, char *out, int cap)
{
    stbsp_snprintf(out, cap, "%s", pw_tr(PW_S_HS_ACC_BLUE + (o >= 0 && o < HM_ACCENTS ? o : 0)));
}

static int bg_count(void)
{
    return HM_BGS;
}
static int get_bg(void)
{
    return hm_cfg_get()->bg;
}
static int set_bg(int o)
{
    if (o == HM_BG_WALLPAPER && !get_wallpaper()) /* none chosen yet: Photos, Square */
        return -1;
    hm_cfg_get()->bg = o;
    return save_home(HS_LOOK_CHANGED);
}
static void text_bg(int o, char *out, int cap)
{
    static const int k[HM_BGS] = {PW_S_HS_BG_AUTO,  PW_S_HS_BG_SKY,    PW_S_HS_BG_GRADIENT,
                                  PW_S_HS_BG_PLAIN, PW_S_HM_WALLPAPER, PW_S_HS_BG_AURORA,
                                  PW_S_HS_BG_NEBULA};

    if (o == HM_BG_WALLPAPER && get_wallpaper())
        stbsp_snprintf(out, cap, "%s", g_wp_name);
    else
        stbsp_snprintf(out, cap, "%s", pw_tr(k[o >= 0 && o < HM_BGS ? o : 0]));
}

static int sky_count(void)
{
    return 13;
}
static int get_sky(void)
{
    return hm_cfg_get()->sky;
}
static int set_sky(int o)
{
    hm_cfg_get()->sky = o;
    return save_home(HS_LOOK_CHANGED);
}
static void text_sky(int o, char *out, int cap)
{
    stbsp_snprintf(out, cap, "%s",
                   o >= 1 && o <= 12 ? pw_tr(PW_S_MONTH_1 + o - 1) : pw_tr(PW_S_HS_SKY_SYSTEM));
}

static int get_wave(void)
{
    return hm_cfg_get()->wave;
}
static int set_wave(int o)
{
    hm_cfg_get()->wave = o;
    return save_home(0);
}
static int get_percent(void)
{
    return hm_cfg_get()->percent;
}
static int set_percent(int o)
{
    hm_cfg_get()->percent = o;
    return save_home(0);
}

static int motion_count(void)
{
    return HM_MOTIONS;
}
static int get_motion(void)
{
    return hm_cfg_get()->motion;
}
static int set_motion(int o)
{
    hm_cfg_get()->motion = o;
    return save_home(0);
}
static void text_motion(int o, char *out, int cap)
{
    static const int k[HM_MOTIONS] = {PW_S_HS_SLIDE_NORMAL, PW_S_HS_MOTION_FAST,
                                      PW_S_HS_MOTION_CALM};

    stbsp_snprintf(out, cap, "%s", pw_tr(k[o >= 0 && o < HM_MOTIONS ? o : 0]));
}

static int get_repeat(void)
{
    return hm_cfg_get()->repeat;
}
static int set_repeat(int o)
{
    hm_cfg_get()->repeat = o;
    return save_home(HS_MUSIC_CHANGED);
}
static void text_repeat(int o, char *out, int cap)
{
    stbsp_snprintf(out, cap, "%s",
                   pw_tr(o == HM_REPEAT_ALL   ? PW_S_HS_REPEAT_ALL
                         : o == HM_REPEAT_ONE ? PW_S_HS_REPEAT_ONE
                                              : PW_S_OFF));
}
static int clock_count(void)
{
    return HM_CLOCKS;
}
static int get_clock(void)
{
    return hm_cfg_get()->clock;
}
static int set_clock(int o)
{
    hm_cfg_get()->clock = o;
    return save_home(0);
}
static void text_clock(int o, char *out, int cap)
{
    static const int k[HM_CLOCKS] = {PW_S_HS_CLOCK_DATE, PW_S_HS_CLOCK_TIME, PW_S_HS_CLOCK_DAY};

    stbsp_snprintf(out, cap, "%s", pw_tr(k[o >= 0 && o < HM_CLOCKS ? o : 0]));
}

static int get_hold(void)
{
    return hm_cfg_get()->hold;
}
static int set_hold(int o)
{
    hm_cfg_get()->hold = o;
    return save_home(0);
}
static void text_hold(int o, char *out, int cap)
{
    stbsp_snprintf(out, cap, "%s",
                   pw_tr(o == HM_HOLD_MUSIC    ? PW_S_HS_HOLD_MUSIC
                         : o == HM_HOLD_ALWAYS ? PW_S_HS_HOLD_ALWAYS
                                               : PW_S_OFF));
}

/* The sleep timer: the music stops and the PSP sleeps that long after it was set (not kept). */
static const int k_timer_min[5] = {0, 15, 30, 60, 90};
static int g_timer;              /* the option chosen */
static unsigned int g_timer_end; /* hm_hw_now_ms() then, 0: none */
static int timer_count(void)
{
    return 5;
}
static int get_timer(void)
{
    return g_timer_end ? g_timer : 0;
}
static int set_timer(int o)
{
    g_timer = o;
    g_timer_end = o > 0 ? (hm_hw_now_ms() + (unsigned int)k_timer_min[o] * 60000u) | 1u : 0;
    return 0;
}
static void text_timer(int o, char *out, int cap)
{
    if (o <= 0 || o >= 5)
        stbsp_snprintf(out, cap, "%s", pw_tr(PW_S_OFF));
    else
        stbsp_snprintf(out, cap, pw_tr(PW_S_HS_TIMER_MIN), k_timer_min[o]);
}

unsigned int hs_timer_end(void)
{
    return g_timer_end;
}

void hs_timer_done(void)
{
    g_timer_end = 0;
}

static int get_shuffle(void)
{
    return hm_cfg_get()->shuffle;
}
static int set_shuffle(int o)
{
    hm_cfg_get()->shuffle = o;
    return save_home(HS_MUSIC_CHANGED);
}

static int get_slide(void)
{
    return pw_config_get()->slide >= 0 && pw_config_get()->slide < 3 ? pw_config_get()->slide : 0;
}
static int set_slide(int o)
{
    pw_config_get()->slide = o;
    return save_config();
}
static void text_slide(int o, char *out, int cap)
{
    static const int k[3] = {PW_S_HS_SLIDE_NORMAL, PW_S_HS_SLIDE_FAST, PW_S_HS_SLIDE_SLOW};

    stbsp_snprintf(out, cap, "%s", pw_tr(k[o >= 0 && o < 3 ? o : 0]));
}

static int get_fast(void)
{
    return pw_config_get()->fast != 0;
}
static int set_fast(int o)
{
    pw_config_get()->fast = o;
    return save_config();
}
static int get_status(void)
{
    return pw_config_get()->status != 0;
}
static int set_status(int o)
{
    pw_config_get()->status = o;
    return save_config();
}

/* Pocketwatch's language: language.txt (as vsh/src/xsettings.c); read once, the rows are built
   every second. */
static int g_lang = -1;
static int lang_count(void)
{
    return pw_lang_name_count;
}
static int get_lang(void)
{
    char buf[16], code[12];
    int n, j = 0;

    if (g_lang >= 0)
        return g_lang;
    n = pw_file_read(DIR "language.txt", buf, sizeof(buf) - 1);
    for (int i = 0; i < n && j < 11; i++) {
        char c = buf[i];

        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_')
            code[j++] = c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
        else if (j)
            break;
    }
    code[j] = 0;
    g_lang = pw_lang_name_index(j ? code : 0);
    return g_lang;
}
static int set_lang(int o)
{
    const char *code = pw_lang_names[o].code;

    if (code)
        pw_file_write(DIR "language.txt", code, (unsigned int)strlen(code), 0);
    else
        pw_file_remove(DIR "language.txt");
    g_lang = -1;
    return HS_TEXTS_CHANGED;
}
static void text_lang(int o, char *out, int cap)
{
    stbsp_snprintf(out, cap, "%s",
                   o <= 0 || o >= pw_lang_name_count ? pw_tr(PW_S_SYSTEM_LANGUAGE)
                                                     : pw_lang_names[o].name);
}

/* The system: sceUtility system parameters (kept by hw.c), each write read back */
static int sys_get(int id)
{
    return hm_hw_param(id);
}
static int sys_set(int id, int v)
{
    return hm_hw_set_param(id, v);
}

static const char *const k_sys_lang_native[12] = {
    0,           "English", "Français", "Español", "Deutsch", "Italiano", "Nederlands",
    "Português", "Русский", 0,          0,         0}; /* 0: no glyphs in the Latin system font -
                                                          named in the UI's language */
static int sys_lang_count(void)
{
    return 12;
}
static int get_sys_lang(void)
{
    return sys_get(PSP_SYSTEMPARAM_ID_INT_LANGUAGE);
}
static int set_sys_lang(int o)
{
    return sys_set(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, o) < 0 ? -1 : HS_TEXTS_CHANGED;
}
static void text_sys_lang(int o, char *out, int cap)
{
    static const int others[12] = {
        PW_S_HS_LANG_JA, 0, 0, 0, 0, 0, 0, 0, 0, PW_S_HS_LANG_KO, PW_S_HS_LANG_ZHT,
        PW_S_HS_LANG_ZHS};

    if (o < 0 || o >= 12)
        stbsp_snprintf(out, cap, "%d", o);
    else
        stbsp_snprintf(out, cap, "%s",
                       k_sys_lang_native[o] ? k_sys_lang_native[o] : pw_tr(others[o]));
}

static int get_date(void)
{
    return sys_get(PSP_SYSTEMPARAM_ID_INT_DATE_FORMAT);
}
static int set_date(int o)
{
    return sys_set(PSP_SYSTEMPARAM_ID_INT_DATE_FORMAT, o);
}
static void text_date(int o, char *out, int cap)
{
    static const char *const f[3] = {"YYYY/MM/DD", "MM/DD/YYYY", "DD/MM/YYYY"};

    stbsp_snprintf(out, cap, "%s", f[o % 3]);
}

static int get_time(void)
{
    return sys_get(PSP_SYSTEMPARAM_ID_INT_TIME_FORMAT);
}
static int set_time(int o)
{
    return sys_set(PSP_SYSTEMPARAM_ID_INT_TIME_FORMAT, o);
}
static void text_time(int o, char *out, int cap)
{
    stbsp_snprintf(out, cap, "%s", pw_tr(o ? PW_S_HS_12H : PW_S_HS_24H));
}

/* The time zones the world uses, in minutes from GMT (the system parameter's unit). */
static const short k_zones[] = {-720, -660, -600, -570, -540, -480, -420, -360, -300, -240,
                                -210, -180, -120, -60,  0,    60,   120,  180,  210,  240,
                                270,  300,  330,  345,  360,  390,  420,  480,  525,  540,
                                570,  600,  630,  660,  720,  765,  780,  840};
#define ZONES ((int)(sizeof(k_zones) / sizeof(k_zones[0])))
static int zone_count(void)
{
    return ZONES;
}
static int get_zone(void)
{
    int m = sys_get(PSP_SYSTEMPARAM_ID_INT_TIMEZONE);

    for (int i = 0; i < ZONES; i++)
        if (k_zones[i] == m)
            return i;
    return -1;
}
static int set_zone(int o)
{
    return sys_set(PSP_SYSTEMPARAM_ID_INT_TIMEZONE, k_zones[o]);
}
static void text_zone(int o, char *out, int cap)
{
    int m = k_zones[o], a = m < 0 ? -m : m;

    stbsp_snprintf(out, cap, "GMT%c%02d:%02d", m < 0 ? '-' : '+', a / 60, a % 60);
}

static int get_summer(void)
{
    return sys_get(PSP_SYSTEMPARAM_ID_INT_DAYLIGHTSAVINGS);
}
static int set_summer(int o)
{
    return sys_set(PSP_SYSTEMPARAM_ID_INT_DAYLIGHTSAVINGS, o);
}
/* The Enter button (system parameter 9: 1 Cross, 0 Circle, as hw.c reads it) */
static int get_enter(void)
{
    return sys_get(PSP_SYSTEMPARAM_ID_INT_UNKNOWN);
}
static int set_enter(int o)
{
    return sys_set(PSP_SYSTEMPARAM_ID_INT_UNKNOWN, o) < 0 ? -1 : HS_KEYS_CHANGED;
}
static void text_enter(int o, char *out, int cap)
{
    stbsp_snprintf(out, cap, "%s", pw_tr(o ? PW_S_HS_BTN_CROSS : PW_S_HS_BTN_CIRCLE));
}

/* The Ad Hoc channel: automatic, 1, 6, 11 */
static const int k_adhoc[4] = {0, 1, 6, 11};
static int adhoc_count(void)
{
    return 4;
}
static int get_adhoc(void)
{
    int v = sys_get(PSP_SYSTEMPARAM_ID_INT_ADHOC_CHANNEL);

    for (int i = 0; i < 4; i++)
        if (k_adhoc[i] == v)
            return i;
    return -1;
}
static int set_adhoc(int o)
{
    return sys_set(PSP_SYSTEMPARAM_ID_INT_ADHOC_CHANNEL, k_adhoc[o]);
}
static void text_adhoc(int o, char *out, int cap)
{
    if (o <= 0)
        stbsp_snprintf(out, cap, "%s", pw_tr(PW_S_HS_AUTOMATIC));
    else
        stbsp_snprintf(out, cap, "%d", k_adhoc[o & 3]);
}

static int get_wlan(void)
{
    return sys_get(PSP_SYSTEMPARAM_ID_INT_WLAN_POWERSAVE);
}
static int set_wlan(int o)
{
    return sys_set(PSP_SYSTEMPARAM_ID_INT_WLAN_POWERSAVE, o);
}

/* The screen and the sound: impose (what the HOME screen's buttons change) */
static int four(void)
{
    return 4;
}
static int get_bright(void)
{
    int v = impose(0, IMPOSE_BRIGHTNESS, 0);

    return v >= 0 && v < 4 ? v : -1;
}
static int set_bright(int o)
{
    return impose(1, IMPOSE_BRIGHTNESS, o) < 0 || get_bright() != o ? -1 : 0;
}
static void text_bright(int o, char *out, int cap)
{
    stbsp_snprintf(out, cap, "%d", o + 1);
}

/* The control centre's brightness slider (ADR-050): the same 0-3 system brightness as Settings. */
int hm_bright_get(void)
{
    return get_bright();
}
int hm_bright_set(int level)
{
    return set_bright(level < 0 ? 0 : level > 3 ? 3 : level);
}
static int get_avls(void)
{
    int v = impose(0, IMPOSE_AVLS, 0);

    return v == 0 || v == 1 ? v : -1;
}
static int set_avls(int o)
{
    return impose(1, IMPOSE_AVLS, o) < 0 || get_avls() != o ? -1 : 0;
}

/* Home's key sounds: sounds.txt "off" (a file of Home's own, RISK-022); none: on. */
#define SOUNDS DIR "sounds.txt"
static int g_sounds = -1;
int hs_sounds(void)
{
    if (g_sounds < 0) {
        char t[8];
        int n = pw_file_read(SOUNDS, t, sizeof(t) - 1);

        g_sounds = !(n >= 3 && memcmp(t, "off", 3) == 0);
    }
    return g_sounds;
}
static int set_sounds(int o)
{
    if (pw_file_write(SOUNDS, o ? "on\n" : "off\n", o ? 3u : 4u, 0) < 0)
        return -1;
    g_sounds = o;
    hm_sound_enable(o);
    return 0;
}

/* The performance overlay (ADR-043): over Home (home.txt), and in games - the module
   pocketwatch_perf.prx, its line in ARK-4's PLUGINS.TXT and what it shows (perf.txt). */
static int get_perf(void)
{
    return hm_cfg_get()->perf ? 1 : 0;
}
static int set_perf(int o)
{
    hm_cfg_get()->perf = o;
    return save_home(0);
}

#define PERF_PRX     DIR "pocketwatch_perf.prx"
#define PERF_CFG     DIR "perf.txt"
#define PLUGINS      "ms0:/seplugins/PLUGINS.TXT"
#define PLUGINS_SIZE 8192
static char g_plugins[2][PLUGINS_SIZE];

/* The overlay module's settings (perf.txt, ADR-045): the numbers in games, the volume and
   brightness indicator, the boost - Home uses the last two too. The module's line in PLUGINS.TXT
   is on while a game needs it: the numbers, the indicator in games, a boost. */
static pw_perf_cfg g_pc;
static int g_pc_read, g_pc_line; /* read; the module's game line is on */

static int pc_write(const pw_perf_cfg *c);
static int pc_line(const char *runlevel, int on);

const pw_perf_cfg *hs_perf_cfg(void)
{
    if (!g_pc_read) {
        char t[128];
        int n = pw_file_read(PLUGINS, g_plugins[0], PLUGINS_SIZE),
            m = pw_file_read(PERF_CFG, t, sizeof(t));

        pw_perf_cfg_parse(t, m > 0 ? m : 0, &g_pc);
        g_pc_line = n > 0 && hm_plugins_get(g_plugins[0], n, "UMD", PERF_PRX) == 1;
        if (!g_pc_line) /* (not loaded: no numbers in games, whatever the file says) */
            g_pc.mode = PW_PERF_OFF;
        g_pc_read = 1;
        /* an old VSH line (ADR-047, before the module became games-only) turned off once */
        if (n > 0 && hm_plugins_get(g_plugins[0], n, "VSH", PERF_PRX) == 1)
            pc_line("VSH", 0);
        { /* the module's one line (games) as the settings want it: once */
            int bright = g_pc.boost > 0 || g_pc.steps;

            if (g_pc_line != (g_pc.mode != PW_PERF_OFF || g_pc.osd == PW_OSD_ALL || bright))
                pc_write(&g_pc);
        }
    }
    return &g_pc;
}

/* One of the module's PLUGINS.TXT lines on or off. */
static int pc_line(const char *runlevel, int on)
{
    int n = pw_file_read(PLUGINS, g_plugins[0], PLUGINS_SIZE),
        m = hm_plugins_set(g_plugins[0], n > 0 ? n : 0, runlevel, PERF_PRX, on, g_plugins[1],
                           PLUGINS_SIZE);

    return m < 0 || pw_file_write(PLUGINS, g_plugins[1], (unsigned int)m, 0) != m ? -1 : 0;
}

static int pc_write(const pw_perf_cfg *c)
{
    static const char *const mode[3] = {"off", "fps", "full"}, *const osd[3] = {"off", "home",
                                                                                "all"};
    char t[192];
    int bright = c->boost > 0 || c->steps,
        line = c->mode != PW_PERF_OFF || c->osd == PW_OSD_ALL || bright, /* games only (ADR-049) */
        k = stbsp_snprintf(t, sizeof(t), "mode = %s\nosd = %s\nboost = %d\nsteps = %s\nlevels =",
                           mode[c->mode % 3], osd[c->osd % 3], c->boost, c->steps ? "on" : "off");

    for (int i = 0; i < c->nlevels && k < (int)sizeof(t) - 8; i++)
        k += stbsp_snprintf(t + k, (int)sizeof(t) - k, " %d", c->levels[i]);
    k += stbsp_snprintf(t + k, (int)sizeof(t) - k, "\n");
    if (pw_file_write(PERF_CFG, t, (unsigned int)k, 0) != k)
        return -1;
    if (line != g_pc_line && pc_line("UMD", line) < 0)
        return -1;
    g_pc_line = line;
    g_pc = *c;
    return 0;
}

static int get_perf_game(void)
{
    return hs_perf_cfg()->mode;
}
static int set_perf_game(int o)
{
    pw_perf_cfg c = *hs_perf_cfg();

    c.mode = o;
    return pc_write(&c) < 0 ? -1 : HS_NEXT_GAME;
}

/* The volume and brightness indicator (HOM-REQ-034): off, over Home, over games too. */
static int get_osd(void)
{
    return hs_perf_cfg()->osd;
}
static int set_osd(int o)
{
    pw_perf_cfg c = *hs_perf_cfg();

    c.osd = o;
    return pc_write(&c) < 0 ? -1 : o == PW_OSD_ALL ? HS_NEXT_GAME : 0;
}
static void text_osd(int o, char *out, int cap)
{
    stbsp_snprintf(out, cap, "%s",
                   pw_tr(o == PW_OSD_HOME  ? PW_S_HS_OSD_HOME
                         : o == PW_OSD_ALL ? PW_S_HS_OSD_ALL
                                           : PW_S_OFF));
}

/* The brightness (ADR-045, ADR-047): the system's steps, boosted halfway up to the most or to
   the most, or Pocketwatch's own steps for SCREEN (perf.txt levels - what the owner's bright3
   plugin was for). Home at once, games and the XMB from their next start. */
static const int k_boost[3] = {0, 50, 100};
static int get_boost(void)
{
    const pw_perf_cfg *c = hs_perf_cfg();

    return c->steps ? 3 : c->boost >= 100 ? 2 : c->boost > 0 ? 1 : 0;
}
static int set_boost(int o)
{
    pw_perf_cfg c = *hs_perf_cfg();

    c.steps = o == 3;
    c.boost = o == 3 ? 0 : k_boost[o % 3];
    return pc_write(&c);
}
static void text_boost(int o, char *out, int cap)
{
    stbsp_snprintf(out, cap, "%s",
                   pw_tr(o == 1   ? PW_S_HS_BOOST_HALF
                         : o == 2 ? PW_S_HS_BOOST_MAX
                         : o == 3 ? PW_S_HS_STEPS
                                  : PW_S_OFF));
}
static void text_perf_game(int o, char *out, int cap)
{
    stbsp_snprintf(out, cap, "%s",
                   pw_tr(o == 1   ? PW_S_HS_PERF_FPS
                         : o == 2 ? PW_S_HS_PERF_FULL
                                  : PW_S_OFF));
}

/* Set Time from Internet (HOM-REQ-032): the last answer, as main.c's thread had it. */
static int g_net_time;
static char g_net_when[16];

void hs_set_net_time(int state, const char *when)
{
    g_net_time = state;
    stbsp_snprintf(g_net_when, sizeof(g_net_when), "%s", when ? when : "");
}

static void text_net_time(char *out, int cap)
{
    out[0] = 0;
    if (g_net_time == HS_NET_ASKING)
        stbsp_snprintf(out, cap, "%s", pw_tr(PW_S_CLK_WORKING));
    else if (g_net_time == HS_NET_DONE)
        stbsp_snprintf(out, cap, pw_tr(PW_S_CLK_SET), g_net_when);
    else if (g_net_time == HS_NET_NO_WIFI)
        stbsp_snprintf(out, cap, "%s", pw_tr(PW_S_CLK_NO_WIFI));
    else if (g_net_time == HS_NET_NO_PROFILE)
        stbsp_snprintf(out, cap, "%s", pw_tr(PW_S_CLK_NO_NET));
    else if (g_net_time == HS_NET_FAILED)
        stbsp_snprintf(out, cap, "%s", pw_tr(PW_S_CLK_FAILED));
}

static const row k_rows[] = {
    {HS_CAT_LOOK, HS_CHOICE, PW_S_SET_STYLE, 0, three, get_style, set_style, text_style},
    {HS_CAT_LOOK, HS_CHOICE, PW_S_HS_THEME, 0, theme_count, get_theme, set_theme, text_theme},
    {HS_CAT_LOOK, HS_CHOICE, PW_S_HS_ACCENT, 0, accent_count, get_accent, set_accent, text_accent},
    {HS_CAT_LOOK, HS_CHOICE, PW_S_HS_BACKGROUND, 0, bg_count, get_bg, set_bg, text_bg},
    {HS_CAT_LOOK, HS_CHOICE, PW_S_HS_SKY, 0, sky_count, get_sky, set_sky, text_sky},
    {HS_CAT_LOOK, HS_CHOICE, PW_S_HS_WAVE, 0, two, get_wave, set_wave, on_off},
    {HS_CAT_LOOK, HS_CHOICE, PW_S_HS_PERCENT, 0, two, get_percent, set_percent, on_off},
    {HS_CAT_LOOK, HS_CHOICE, PW_S_HS_MOTION, 0, motion_count, get_motion, set_motion, text_motion},
    {HS_CAT_LOOK, HS_CHOICE, PW_S_HS_CLOCK, 0, clock_count, get_clock, set_clock, text_clock},
    {HS_CAT_POCKETWATCH, HS_CHOICE, PW_S_SET_START, 0, two, get_start, set_start, text_start},
    {HS_CAT_POCKETWATCH, HS_CHOICE, PW_S_HS_SORT, 0, two, get_sort, set_sort, text_sort},
    {HS_CAT_POCKETWATCH, HS_CHOICE, PW_S_SET_FAST, 0, two, get_fast, set_fast, on_off},
    {HS_CAT_POCKETWATCH, HS_CHOICE, PW_S_SET_STATUS, 0, two, get_status, set_status, on_off},
    {HS_CAT_POCKETWATCH, HS_CHOICE, PW_S_HS_PERF, 0, two, get_perf, set_perf, on_off},
    {HS_CAT_POCKETWATCH, HS_CHOICE, PW_S_HS_PERF_GAME, 0, three, get_perf_game, set_perf_game,
     text_perf_game},
    {HS_CAT_MEDIA, HS_CHOICE, PW_S_HS_REPEAT, 0, three, get_repeat, set_repeat, text_repeat},
    {HS_CAT_MEDIA, HS_CHOICE, PW_S_HS_SHUFFLE, 0, two, get_shuffle, set_shuffle, on_off},
    {HS_CAT_MEDIA, HS_CHOICE, PW_S_HS_TIMER, 0, timer_count, get_timer, set_timer, text_timer},
    {HS_CAT_MEDIA, HS_CHOICE, PW_S_HS_SLIDESHOW, 0, three, get_slide, set_slide, text_slide},
    {HS_CAT_SOUND, HS_CHOICE, PW_S_HS_SOUNDS, 0, two, hs_sounds, set_sounds, on_off},
    {HS_CAT_SOUND, HS_CHOICE, PW_S_HS_BRIGHTNESS, 0, four, get_bright, set_bright, text_bright},
    {HS_CAT_SOUND, HS_CHOICE, PW_S_HS_BOOST, 0, four, get_boost, set_boost, text_boost},
    {HS_CAT_SOUND, HS_CHOICE, PW_S_HS_OSD, 0, three, get_osd, set_osd, text_osd},
    {HS_CAT_SOUND, HS_CHOICE, PW_S_HS_AVLS, 0, two, get_avls, set_avls, on_off},
    {HS_CAT_SOUND, HS_CHOICE, PW_S_HS_HOLD, 0, three, get_hold, set_hold, text_hold},
    {HS_CAT_TIME, HS_CHOICE, PW_S_HS_DATE_FORMAT, 0, three, get_date, set_date, text_date},
    {HS_CAT_TIME, HS_CHOICE, PW_S_HS_TIME_FORMAT, 0, two, get_time, set_time, text_time},
    {HS_CAT_TIME, HS_CHOICE, PW_S_HS_TIME_ZONE, 0, zone_count, get_zone, set_zone, text_zone},
    {HS_CAT_TIME, HS_CHOICE, PW_S_HS_SUMMER_TIME, 0, two, get_summer, set_summer, on_off},
    {HS_CAT_TIME, HS_ACTION, PW_S_CLK_NET, HS_ACT_NET_TIME, 0, 0, 0, 0},
    {HS_CAT_SYSTEM, HS_CHOICE, PW_S_SET_LANGUAGE, 0, lang_count, get_lang, set_lang, text_lang},
    {HS_CAT_SYSTEM, HS_CHOICE, PW_S_HS_SYS_LANGUAGE, 0, sys_lang_count, get_sys_lang, set_sys_lang,
     text_sys_lang},
    {HS_CAT_SYSTEM, HS_ACTION, PW_S_HS_NICKNAME, HS_ACT_NICKNAME, 0, 0, 0, 0},
    {HS_CAT_SYSTEM, HS_CHOICE, PW_S_HS_ENTER, 0, two, get_enter, set_enter, text_enter},
    {HS_CAT_NETWORK, HS_CHOICE, PW_S_HS_WLAN_SAVE, 0, two, get_wlan, set_wlan, on_off},
    {HS_CAT_NETWORK, HS_CHOICE, PW_S_HS_ADHOC, 0, adhoc_count, get_adhoc, set_adhoc, text_adhoc},
};
#define ROWS ((int)(sizeof(k_rows) / sizeof(k_rows[0])))

int hs_count(void)
{
    return ROWS;
}

static const struct {
    int label, glyph, art;
} k_cats[HS_CATS] = {
    {PW_S_HS_CAT_LOOK, HM_ICON_PALETTE, HM_ART_THEME + 1},
    {PW_S_HS_POCKETWATCH, HM_ICON_GAMES, HM_ART_UMD + 1}, /* (PW_ICON_WATCH is 0: none) */
    {PW_S_HS_CAT_MEDIA, HM_ICON_MUSIC, HM_ART_PHOTO + 1},
    {PW_S_HS_CAT_SOUND, HM_ICON_SPEAKER, HM_ART_SOUND + 1},
    {PW_S_HS_CAT_TIME, HM_ICON_CLOCK, HM_ART_CLOCK + 1},
    {PW_S_HM_SYSTEM, HM_ICON_LANGUAGE, HM_ART_SYSTEM + 1},
    {PW_S_HS_CAT_NETWORK, HM_ICON_GLOBE, HM_ART_NETWORK + 1},
};

const char *hs_cat_label(int c)
{
    return c >= 0 && c < HS_CATS ? pw_tr(k_cats[c].label) : "";
}

int hs_cat_glyph(int c)
{
    return c >= 0 && c < HS_CATS ? k_cats[c].glyph : 0;
}

int hs_cat_art(int c)
{
    return c >= 0 && c < HS_CATS ? k_cats[c].art : 0;
}

int hs_cat_rows(int c, int *idx, int max)
{
    int n = 0;

    for (int i = 0; i < ROWS && n < max; i++)
        if (k_rows[i].cat == c)
            idx[n++] = i;
    return n;
}

void hs_cat_value(int c, char *out, int cap)
{
    int idx[1];

    out[0] = 0;
    if (c == HS_CAT_LOOK && hs_cat_rows(c, idx, 1) == 1) /* the style: what this page is about */
        hs_value(idx[0], out, cap);
    else if (c == HS_CAT_SYSTEM) /* the language */
        for (int i = 0; i < ROWS; i++)
            if (k_rows[i].label == PW_S_SET_LANGUAGE)
                hs_value(i, out, cap);
}

unsigned int hs_swatch(int i, int o)
{
    if (i < 0 || i >= ROWS || o < 0)
        return 0;
    if (k_rows[i].label == PW_S_HS_ACCENT)
        return hm_cfg_accent(o);
    if (k_rows[i].label == PW_S_HS_SKY && o >= 1 && o <= 12)
        return g_sky_colour[o - 1];
    return 0;
}

int hs_kind(int i)
{
    return i >= 0 && i < ROWS ? k_rows[i].kind : HS_HEADER;
}

int hs_action(int i)
{
    return i >= 0 && i < ROWS ? k_rows[i].action : HS_ACT_NONE;
}

const char *hs_label(int i)
{
    return i >= 0 && i < ROWS ? pw_tr(k_rows[i].label) : "";
}

void hs_value(int i, char *out, int cap)
{
    const row *r = i >= 0 && i < ROWS ? &k_rows[i] : 0;
    int v;

    out[0] = 0;
    if (!r || r->kind == HS_HEADER)
        return;
    if (r->action == HS_ACT_NICKNAME) {
        hs_nickname(out, cap);
        return;
    }
    if (r->action == HS_ACT_NET_TIME) {
        text_net_time(out, cap);
        return;
    }
    v = r->get();
    if (v < 0 || v >= r->count())
        stbsp_snprintf(out, cap, "%s", pw_tr(PW_S_BAT_NA));
    else
        r->text(v, out, cap);
}

int hs_step(int i, int dir)
{
    const row *r = i >= 0 && i < ROWS ? &k_rows[i] : 0;
    int n, v;

    if (!r || r->kind != HS_CHOICE)
        return 0;
    n = r->count();
    v = r->get();
    if (v < 0) /* unknown now: start from the first */
        v = dir > 0 ? -1 : 0;
    v = (v + dir + n) % n;
    return r->set(v);
}

/* Kept as hw.c keeps the other parameters: read again only after Home wrote it. */
static char g_nick[64];
static int g_nick_read;
void hs_nickname(char *out, int cap)
{
    if (!g_nick_read) {
        if (sceUtilityGetSystemParamString(PSP_SYSTEMPARAM_ID_STRING_NICKNAME, g_nick,
                                           sizeof(g_nick)) != 0)
            g_nick[0] = 0;
        g_nick[sizeof(g_nick) - 1] = 0;
        g_nick_read = 1;
    }
    stbsp_snprintf(out, cap, "%s", g_nick);
}

int hs_set_nickname(const char *utf8)
{
    char back[64];
    int r = utf8[0] ? sceUtilitySetSystemParamString(PSP_SYSTEMPARAM_ID_STRING_NICKNAME, utf8) : -1;

    g_nick_read = 0;
    if (r != 0)
        return -1;
    hs_nickname(back, sizeof(back));
    return strcmp(back, utf8) == 0 ? 0 : -1;
}

int hs_toggle(int i)
{
    const row *r = i >= 0 && i < ROWS ? &k_rows[i] : 0;
    int v;

    if (!r || r->kind != HS_CHOICE || r->text != on_off)
        return 0;
    v = r->get();
    return v == 0 ? 1 : v == 1 ? 2 : 0;
}

int hs_options(int i)
{
    return i >= 0 && i < ROWS && k_rows[i].kind == HS_CHOICE ? k_rows[i].count() : 0;
}

void hs_option_text(int i, int o, char *out, int cap)
{
    out[0] = 0;
    if (o >= 0 && o < hs_options(i))
        k_rows[i].text(o, out, cap);
}

int hs_current(int i)
{
    return hs_options(i) > 0 ? k_rows[i].get() : -1;
}

int hs_choose(int i, int o)
{
    if (o < 0 || o >= hs_options(i))
        return 0;
    return o == k_rows[i].get() ? 0 : k_rows[i].set(o);
}

int hs_next(int i, int dir)
{
    for (int j = i + dir; j >= 0 && j < ROWS; j += dir)
        if (k_rows[j].kind != HS_HEADER)
            return j;
    return i;
}

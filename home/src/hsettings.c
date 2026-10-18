// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspkernel.h>
#include <psputility_sysparam.h>
#include <string.h>

#include "core/config.h"
#include "core/platform.h"
#include "hsettings.h"
#include "sound.h"
#include "stb_sprintf.h"
#include "ui/i18n.h"
#include "ui/langnames.h"

#define DIR "ms0:/seplugins/pocketwatch/"

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
    return save_config();
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
static int set_wallpaper(int o)
{
    if (o != 0) /* a picture is chosen in Photos */
        return get_wallpaper() ? 0 : -1;
    pw_file_remove(WALLPAPER_BIN);
    pw_file_remove(WALLPAPER);
    g_wp_known = 1;
    return HS_WALLPAPER_CHANGED;
}
static void text_wallpaper(int o, char *out, int cap)
{
    stbsp_snprintf(out, cap, "%s", o && get_wallpaper() ? g_wp_name : pw_tr(PW_S_HM_WP_DEFAULT));
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
    return 0;
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

/* Pocketwatch's language: language.txt (as vsh/src/xsettings.c) */
static int lang_count(void)
{
    return pw_lang_name_count;
}
static int get_lang(void)
{
    char buf[16], code[12];
    int n = pw_file_read(DIR "language.txt", buf, sizeof(buf) - 1), j = 0;

    for (int i = 0; i < n && j < 11; i++) {
        char c = buf[i];

        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_')
            code[j++] = c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
        else if (j)
            break;
    }
    code[j] = 0;
    return pw_lang_name_index(j ? code : 0);
}
static int set_lang(int o)
{
    const char *code = pw_lang_names[o].code;

    if (code)
        pw_file_write(DIR "language.txt", code, (unsigned int)strlen(code), 0);
    else
        pw_file_remove(DIR "language.txt");
    return HS_TEXTS_CHANGED;
}
static void text_lang(int o, char *out, int cap)
{
    stbsp_snprintf(out, cap, "%s",
                   o <= 0 || o >= pw_lang_name_count ? pw_tr(PW_S_SYSTEM_LANGUAGE)
                                                     : pw_lang_names[o].name);
}

/* The system: sceUtility system parameters, each write read back */
static int sys_get(int id)
{
    int v = 0;

    return sceUtilityGetSystemParamInt(id, &v) == 0 ? v : -1;
}
static int sys_set(int id, int v)
{
    return sceUtilitySetSystemParamInt(id, v) == 0 && sys_get(id) == v ? 0 : -1;
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

static const row k_rows[] = {
    {HS_HEADER, PW_S_HS_POCKETWATCH, 0, 0, 0, 0, 0},
    {HS_CHOICE, PW_S_SET_START, 0, two, get_start, set_start, text_start},
    {HS_CHOICE, PW_S_HS_SORT, 0, two, get_sort, set_sort, text_sort},
    {HS_CHOICE, PW_S_HM_WALLPAPER, 0, two, get_wallpaper, set_wallpaper, text_wallpaper},
    {HS_CHOICE, PW_S_HS_SLIDESHOW, 0, three, get_slide, set_slide, text_slide},
    {HS_CHOICE, PW_S_SET_STYLE, 0, three, get_style, set_style, text_style},
    {HS_CHOICE, PW_S_SET_FAST, 0, two, get_fast, set_fast, on_off},
    {HS_CHOICE, PW_S_SET_STATUS, 0, two, get_status, set_status, on_off},
    {HS_CHOICE, PW_S_SET_LANGUAGE, 0, lang_count, get_lang, set_lang, text_lang},
    {HS_HEADER, PW_S_HM_SYSTEM, 0, 0, 0, 0, 0},
    {HS_ACTION, PW_S_HS_NICKNAME, HS_ACT_NICKNAME, 0, 0, 0, 0},
    {HS_CHOICE, PW_S_HS_SYS_LANGUAGE, 0, sys_lang_count, get_sys_lang, set_sys_lang, text_sys_lang},
    {HS_CHOICE, PW_S_HS_DATE_FORMAT, 0, three, get_date, set_date, text_date},
    {HS_CHOICE, PW_S_HS_TIME_FORMAT, 0, two, get_time, set_time, text_time},
    {HS_CHOICE, PW_S_HS_TIME_ZONE, 0, zone_count, get_zone, set_zone, text_zone},
    {HS_CHOICE, PW_S_HS_SUMMER_TIME, 0, two, get_summer, set_summer, on_off},
    {HS_CHOICE, PW_S_HS_WLAN_SAVE, 0, two, get_wlan, set_wlan, on_off},
    {HS_HEADER, PW_S_HS_SCREEN_SOUND, 0, 0, 0, 0, 0},
    {HS_CHOICE, PW_S_HS_BRIGHTNESS, 0, four, get_bright, set_bright, text_bright},
    {HS_CHOICE, PW_S_HS_AVLS, 0, two, get_avls, set_avls, on_off},
    {HS_CHOICE, PW_S_HS_SOUNDS, 0, two, hs_sounds, set_sounds, on_off},
};
#define ROWS ((int)(sizeof(k_rows) / sizeof(k_rows[0])))

int hs_count(void)
{
    return ROWS;
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

void hs_nickname(char *out, int cap)
{
    if (sceUtilityGetSystemParamString(PSP_SYSTEMPARAM_ID_STRING_NICKNAME, out, cap) != 0)
        out[0] = 0;
}

int hs_set_nickname(const char *utf8)
{
    char back[64];

    if (!utf8[0] || sceUtilitySetSystemParamString(PSP_SYSTEMPARAM_ID_STRING_NICKNAME, utf8) != 0)
        return -1;
    hs_nickname(back, sizeof(back));
    return strcmp(back, utf8) == 0 ? 0 : -1;
}

int hs_next(int i, int dir)
{
    for (int j = i + dir; j >= 0 && j < ROWS; j += dir)
        if (k_rows[j].kind != HS_HEADER)
            return j;
    return i;
}

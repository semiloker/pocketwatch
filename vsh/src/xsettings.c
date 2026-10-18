// SPDX-License-Identifier: GPL-3.0-or-later
#include <psputility_sysparam.h>
#include <string.h>

#include "core/battmodel.h"
#include "core/config.h"
#include "core/platform.h"
#include "core/playtime.h"
#include "stb_sprintf.h"
#include "ui/i18n.h"
#include "ui/langnames.h"
#include "xbattery.h"
#include "xlog.h"
#include "xsettings.h"

#define U        PW_BATT_UNKNOWN
#define LANG_MAX (8 * 1024) /* the largest translation is 4 KB (ui/lang.c takes 32) */
#define CODE_MAX 12

void *sce_paf_private_malloc(int size); /* src/stubs.S */
void sce_paf_private_free(void *p);

/* What a row is: one of the four settings, or a reading (one value). */
enum { ROW_START, ROW_STYLE, ROW_FAST, ROW_STATUS, ROW_LANGUAGE, ROW_INFO };

typedef struct {
    int kind;
    const char *label, *value; /* value: ROW_INFO only */
} row;

static const char *const g_keys[PWX_ROWS_MAX] = {
    "pw_r00", "pw_r01", "pw_r02", "pw_r03", "pw_r04", "pw_r05", "pw_r06", "pw_r07", "pw_r08",
    "pw_r09", "pw_r10", "pw_r11", "pw_r12", "pw_r13", "pw_r14", "pw_r15", "pw_r16", "pw_r17"};

static row g_rows[PWX_ROWS_MAX];
static char g_text[PWX_ROWS_MAX][2][56]; /* labels and values of reading rows */
static int g_n, g_page;
static char g_code[CODE_MAX]; /* language.txt's code, "" for the system's */
static char g_lang_text[LANG_MAX + 1];

/* ---- texts and settings ---- */

static void read_code(void)
{
    char buf[CODE_MAX + 4];
    int n = pw_file_read(PWX_DIR "language.txt", buf, sizeof(buf) - 1), j = 0;

    for (int i = 0; i < n && j < CODE_MAX - 1; i++) { /* as ui/lang.c read_override */
        char c = buf[i];

        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_')
            g_code[j++] = c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
        else if (j > 0)
            break;
    }
    g_code[j] = 0;
}

static void load_lang(void)
{
    const char *code = g_code[0] ? g_code : 0;
    char path[96];
    int lang = 1, n;

    pw_i18n_reset();
    if (!code && sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &lang) == 0)
        code = pw_i18n_code(lang);
    if (!code || strcmp(code, "en") == 0)
        return;
    strcpy(path, PWX_DIR "lang/");
    strncat(path, code, CODE_MAX);
    strcat(path, ".txt");
    n = pw_file_read(path, g_lang_text, LANG_MAX);
    if (n <= 0 || pw_i18n_load(g_lang_text, (unsigned int)n, 0, 0) < 0)
        pwx_log("xmb: %s unusable (%d): English", path, n);
}

void pwx_texts_load(void)
{
    read_code();
    load_lang();
}

/* ---- rows ---- */

static void add(int kind, const char *label, const char *value)
{
    if (g_n < PWX_ROWS_MAX) {
        g_rows[g_n].kind = kind;
        g_rows[g_n].label = label;
        g_rows[g_n].value = value;
        g_n++;
    }
}

/* Storage for a reading row's label (0) or value (1). */
static char *text(int which)
{
    return g_text[g_n < PWX_ROWS_MAX ? g_n : PWX_ROWS_MAX - 1][which];
}

static void duration(char *out, int cap, unsigned int minutes)
{
    if (minutes >= 60)
        stbsp_snprintf(out, cap, pw_tr(PW_S_DUR_HM), (int)(minutes / 60), (int)(minutes % 60));
    else
        stbsp_snprintf(out, cap, pw_tr(PW_S_DUR_M), (int)minutes);
}

static const char *state_text(int state)
{
    switch (state) {
    case PW_BATT_NONE:
        return pw_tr(PW_S_BAT_NONE);
    case PW_BATT_CHARGING:
        return pw_tr(PW_S_BAT_CHARGING);
    case PW_BATT_FULL:
        return pw_tr(PW_S_BAT_ON_POWER);
    case PW_BATT_DISCHARGING:
        return pw_tr(PW_S_BAT_ON_BATTERY);
    default:
        return pw_tr(PW_S_BAT_NA);
    }
}

/* A reading row: label, then the value printed with fmt from a and b, or "Not available". */
static void reading(const char *label, const char *fmt, int a, int b, int known)
{
    char *v = text(1);

    if (known)
        stbsp_snprintf(v, 56, fmt, a, b);
    add(ROW_INFO, label, known ? v : pw_tr(PW_S_BAT_NA));
}

static void battery_page(void)
{
    pw_batt_sample s;
    pw_batt_estimate e;
    int ma;

    pwx_battery_read(&s); /* a fresh reading; the estimate uses the thread's history */
    pw_batt_estimate_now(pwx_battery_history(), &e);
    if (!pw_batt_history_last(pwx_battery_history()))
        e.state = pw_batt_state(&s); /* the thread has no reading yet */
    ma = s.ma == U ? U : s.ma < 0 ? -s.ma : s.ma;
    reading(pw_tr(PW_S_BAT_CHARGE), "%d%%", s.percent, 0, s.percent != U);
    add(ROW_INFO, pw_tr(PW_S_BAT_STATUS), state_text(e.state));
    if (e.state == PW_BATT_DISCHARGING || e.state == PW_BATT_CHARGING) {
        char d[40], *v = text(1);

        if (e.minutes < 0) {
            stbsp_snprintf(v, 56, "%s", pw_tr(PW_S_BAT_ESTIMATING));
        } else {
            duration(d, sizeof(d), (unsigned int)e.minutes);
            stbsp_snprintf(
                v, 56, pw_tr(e.confidence == PW_BATT_EST_SYSTEM ? PW_S_BAT_SYSTEM : PW_S_BAT_ABOUT),
                d);
        }
        add(ROW_INFO, pw_tr(e.state == PW_BATT_CHARGING ? PW_S_BAT_TO_FULL : PW_S_BAT_LEFT), v);
    }
    reading(pw_tr(PW_S_BAT_VOLTAGE), "%d.%02d V", s.mv / 1000, s.mv % 1000 / 10, s.mv != U);
    reading(pw_tr(PW_S_BAT_CURRENT), "%d mA", ma, 0, ma != U);
    reading(pw_tr(PW_S_BAT_TEMPERATURE),
            "%d \xC2\xB0"
            "C",
            s.temp_c, 0, s.temp_c != U);
    if (s.rcap_mah != U && s.full_mah != U) {
        char *v = text(1);

        stbsp_snprintf(v, 56, pw_tr(PW_S_BAT_CAPACITY_V), s.rcap_mah, s.full_mah);
        add(ROW_INFO, pw_tr(PW_S_BAT_CAPACITY), v);
    } else {
        add(ROW_INFO, pw_tr(PW_S_BAT_CAPACITY), pw_tr(PW_S_BAT_NA));
    }
    reading(pw_tr(PW_S_BAT_CYCLES), "%d", s.cycles, 0, s.cycles != U);
}

/* The play-time table from the newer complete file (as complications/playtime/ writes them). */
static int read_table(pw_pt_table *t, char *buf)
{
    unsigned int best = 0;
    int which = -1;

    for (int i = 0; i < 2; i++) {
        int n = pw_file_read(i ? PWX_DIR "playtime-b.txt" : PWX_DIR "playtime-a.txt", buf,
                             PW_PT_TEXT_MAX);

        if (n > 0 && pw_pt_parse(t, buf, (unsigned int)n) >= 0 &&
            (which < 0 || t->seq + 1 > best)) {
            best = t->seq + 1;
            which = i;
        }
    }
    if (which < 0)
        return -1;
    {
        int n = pw_file_read(which ? PWX_DIR "playtime-b.txt" : PWX_DIR "playtime-a.txt", buf,
                             PW_PT_TEXT_MAX);

        return n > 0 ? pw_pt_parse(t, buf, (unsigned int)n) : -1;
    }
}

static void playtime_page(void)
{
    pw_pt_table *t = sce_paf_private_malloc(sizeof(pw_pt_table));
    char *buf = sce_paf_private_malloc(PW_PT_TEXT_MAX);
    int order[PW_PT_GAMES_MAX], n = 0;
    unsigned int total = 0;

    if (t && buf && read_table(t, buf) > 0)
        n = pw_pt_order(t, order, PW_PT_GAMES_MAX);
    if (n == 0) {
        add(ROW_INFO, pw_tr(PW_S_PT_NONE), " ");
    } else {
        char *v;

        for (int i = 0; i < t->n; i++)
            total += t->g[i].total_s;
        v = text(1);
        duration(v, 56, total / 60);
        add(ROW_INFO, pw_tr(PW_S_PT_TOTAL), v);
        for (int i = 0; i < n && g_n < PWX_ROWS_MAX - 1; i++) {
            const pw_pt_game *g = &t->g[order[i]];
            char *label = text(0);

            stbsp_snprintf(label, 56, "%s", g->title[0] ? g->title : g->id);
            v = text(1);
            duration(v, 56, g->total_s / 60);
            add(ROW_INFO, label, v);
        }
    }
    add(ROW_INFO, pw_tr(PW_S_PT_NOTE), " ");
    if (buf)
        sce_paf_private_free(buf);
    if (t)
        sce_paf_private_free(t);
}

void pwx_page_open(int page)
{
    g_page = page;
    g_n = 0;
    if (page == PWX_PAGE_NONE)
        return;
    pw_config_load(PWX_DIR "config.txt");
    pwx_texts_load();
    if (page == PWX_PAGE_SETTINGS) {
        add(ROW_START, pw_tr(PW_S_SET_START), 0);
        add(ROW_STYLE, pw_tr(PW_S_SET_STYLE), 0);
        add(ROW_FAST, pw_tr(PW_S_SET_FAST), 0);
        add(ROW_STATUS, pw_tr(PW_S_SET_STATUS), 0);
        add(ROW_LANGUAGE, pw_tr(PW_S_SET_LANGUAGE), 0);
    } else if (page == PWX_PAGE_BATTERY) {
        battery_page();
    } else {
        playtime_page();
    }
}

int pwx_page(void)
{
    return g_page;
}

const char *pwx_page_title(void)
{
    return g_page == PWX_PAGE_BATTERY    ? pw_tr(PW_S_BATTERY)
           : g_page == PWX_PAGE_PLAYTIME ? pw_tr(PW_S_PLAYTIME)
                                         : "Pocketwatch";
}

int pwx_rows(void)
{
    return g_n;
}

int pwx_row_by_key(const char *key)
{
    for (int i = 0; key && i < g_n; i++)
        if (strcmp(key, g_keys[i]) == 0)
            return i;
    return -1;
}

const char *pwx_row_key(int row)
{
    return g_keys[row];
}

const char *pwx_row_label(int row)
{
    return g_rows[row].label;
}

int pwx_row_count(int row)
{
    switch (g_rows[row].kind) {
    case ROW_STYLE:
        return 3;
    case ROW_LANGUAGE:
        return pw_lang_name_count;
    case ROW_INFO:
        return 1;
    default:
        return 2;
    }
}

const char *pwx_row_option(int row, int i)
{
    static const int styles[3] = {PW_S_STYLE_XMB, PW_S_STYLE_MODERN, PW_S_STYLE_MINIMAL};

    switch (g_rows[row].kind) {
    case ROW_STYLE:
        return pw_tr(styles[i % 3]);
    case ROW_START:
        return pw_tr(i ? PW_S_START_HOME : PW_S_START_XMB);
    case ROW_LANGUAGE:
        return i <= 0 || i >= pw_lang_name_count ? pw_tr(PW_S_SYSTEM_LANGUAGE)
                                                 : pw_lang_names[i].name;
    case ROW_INFO:
        return g_rows[row].value;
    default:
        return pw_tr(i ? PW_S_ON : PW_S_OFF);
    }
}

int pwx_row_get(int row)
{
    const pw_config *c = pw_config_get();

    switch (g_rows[row].kind) {
    case ROW_START:
        return c->start == 1;
    case ROW_STYLE:
        return c->style;
    case ROW_FAST:
        return c->fast != 0;
    case ROW_STATUS:
        return c->status != 0;
    case ROW_LANGUAGE:
        return pw_lang_name_index(g_code[0] ? g_code : 0);
    default:
        return 0;
    }
}

void pwx_row_set(int row, int value)
{
    pw_config *c = pw_config_get();
    int kind = g_rows[row].kind;

    if (kind == ROW_INFO || value < 0 || value >= pwx_row_count(row))
        return; /* a reading: nothing to choose */
    if (kind == ROW_LANGUAGE) {
        const char *code = pw_lang_names[value].code;

        if (code) {
            pw_file_write(PWX_DIR "language.txt", code, (unsigned int)strlen(code), 0);
            strcpy(g_code, code);
        } else {
            pw_file_remove(PWX_DIR "language.txt");
            g_code[0] = 0;
        }
        load_lang();
        pwx_log("xmb: language %s", code ? code : "of the system");
        return;
    }
    if (kind == ROW_START)
        c->start = value;
    else if (kind == ROW_STYLE)
        c->style = value;
    else if (kind == ROW_FAST)
        c->fast = value;
    else
        c->status = value;
    if (pw_config_save(PWX_DIR "config.txt") < 0)
        pwx_log("xmb: config.txt could not be written");
}

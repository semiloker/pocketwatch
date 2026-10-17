// SPDX-License-Identifier: GPL-3.0-or-later
#include <psputility_sysparam.h>
#include <string.h>

#include "core/config.h"
#include "core/platform.h"
#include "ui/i18n.h"
#include "ui/langnames.h"
#include "xlog.h"
#include "xsettings.h"

#define LANG_MAX (8 * 1024) /* the largest translation is 4 KB (ui/lang.c takes 32) */
#define CODE_MAX 12

static const char *const g_keys[PWX_ROW_COUNT] = {"pw_style", "pw_fast", "pw_status",
                                                  "pw_language"};
static const int g_labels[PWX_ROW_COUNT] = {PW_S_SET_STYLE, PW_S_SET_FAST, PW_S_SET_STATUS,
                                            PW_S_SET_LANGUAGE};
static char g_code[CODE_MAX]; /* language.txt's code, "" for the system's */
static char g_lang_text[LANG_MAX + 1];

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

static void load_texts(void)
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

void pwx_settings_load(void)
{
    pw_config_load(PWX_DIR "config.txt");
    read_code();
    load_texts();
}

int pwx_row_by_key(const char *key)
{
    for (int i = 0; key && i < PWX_ROW_COUNT; i++)
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
    return pw_tr(g_labels[row]);
}

int pwx_row_count(int row)
{
    return row == PWX_ROW_STYLE ? 3 : row == PWX_ROW_LANGUAGE ? pw_lang_name_count : 2;
}

const char *pwx_row_option(int row, int i)
{
    static const int styles[3] = {PW_S_STYLE_XMB, PW_S_STYLE_MODERN, PW_S_STYLE_MINIMAL};

    if (row == PWX_ROW_STYLE)
        return pw_tr(styles[i % 3]);
    if (row == PWX_ROW_LANGUAGE)
        return i <= 0 || i >= pw_lang_name_count ? pw_tr(PW_S_SYSTEM_LANGUAGE)
                                                 : pw_lang_names[i].name;
    return pw_tr(i ? PW_S_ON : PW_S_OFF);
}

int pwx_row_get(int row)
{
    const pw_config *c = pw_config_get();

    switch (row) {
    case PWX_ROW_STYLE:
        return c->style;
    case PWX_ROW_FAST:
        return c->fast != 0;
    case PWX_ROW_STATUS:
        return c->status != 0;
    default:
        return pw_lang_name_index(g_code[0] ? g_code : 0);
    }
}

void pwx_row_set(int row, int value)
{
    pw_config *c = pw_config_get();

    if (value < 0 || value >= pwx_row_count(row))
        return;
    if (row == PWX_ROW_LANGUAGE) {
        const char *code = pw_lang_names[value].code;

        if (code) {
            pw_file_write(PWX_DIR "language.txt", code, (unsigned int)strlen(code), 0);
            strcpy(g_code, code);
        } else {
            pw_file_remove(PWX_DIR "language.txt");
            g_code[0] = 0;
        }
        load_texts();
        pwx_log("xmb: language %s", code ? code : "of the system");
        return;
    }
    if (row == PWX_ROW_STYLE)
        c->style = value;
    else if (row == PWX_ROW_FAST)
        c->fast = value;
    else
        c->status = value;
    if (pw_config_save(PWX_DIR "config.txt") < 0)
        pwx_log("xmb: config.txt could not be written");
}

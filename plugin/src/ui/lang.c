// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "core/log.h"
#include "core/mem.h"
#include "core/platform.h"
#include "gfx/assets.h"
#include "gfx/text.h"
#include "stb_sprintf.h"
#include "ui/i18n.h"
#include "ui/lang.h"

int sceImposeGetLanguageMode(int *lang, int *button); /* core/stubs/sceImpose_driver.S */

#define LANG_MAX  (32 * 1024)
#define CODE_MAX  12
#define PATH_MAX_ 256

static char g_chosen[CODE_MAX]; /* language.txt's code, "" for the system language */

static int covers(const char *s, void *ctx)
{
    (void)ctx;
    return pw_text_covers(pw_assets_font(PW_FONT_TITLE), s) &&
           pw_text_covers(pw_assets_font(PW_FONT_BODY), s);
}

static void plugin_dir(char *dir, unsigned int cap)
{
    if (pw_log_dir(dir, cap) < 0)
        strcpy(dir, "ms0:/seplugins/pocketwatch/");
}

/* language.txt: one code ("fr", "uk", "en") that wins over the system language. */
static int read_override(const char *dir, char *code)
{
    char path[PATH_MAX_], buf[CODE_MAX + 4];
    int n, j = 0;

    stbsp_snprintf(path, sizeof(path), "%slanguage.txt", dir);
    n = pw_file_read(path, buf, sizeof(buf) - 1);
    if (n <= 0)
        return 0;
    for (int i = 0; i < n && j < CODE_MAX - 1; i++) {
        char c = buf[i];

        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == '_')
            code[j++] = c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
        else if (j > 0)
            break;
    }
    code[j] = 0;
    return j > 0;
}

static const char *system_code(int *lang)
{
    int button = 1;

    *lang = 1;
    return sceImposeGetLanguageMode(lang, &button) >= 0 ? pw_i18n_code(*lang) : 0;
}

/* Take lang/<code>.txt; English when code is NULL or "en", or when the file is unusable. */
static void load(const char *dir, const char *code)
{
    char path[PATH_MAX_];
    pw_mem_block mem;
    int size, n, ret;

    pw_i18n_reset();
    if (code == 0 || strcmp(code, "en") == 0) {
        pw_log("i18n: English");
        return;
    }
    stbsp_snprintf(path, sizeof(path), "%slang/%s.txt", dir, code);
    size = pw_file_size(path);
    if (size <= 0 || size > LANG_MAX) {
        pw_log("i18n: %s not found (0x%08X): English", path, size);
        return;
    }
    if (pw_mem_alloc(&mem, "PwLang", (unsigned int)size + 1) < 0) {
        pw_log("i18n: no memory for %s: English", path);
        return;
    }
    n = pw_file_read(path, mem.ptr, (unsigned int)size);
    ret = n > 0 ? pw_i18n_load(mem.ptr, (unsigned int)n, covers, 0) : -1;
    pw_mem_free(&mem);
    if (ret < 0)
        pw_log("i18n: %s %s: English", path,
               ret == -2   ? "has letters the system font lacks"
               : ret == -3 ? "is too long"
                           : "has no usable line");
    else
        pw_log("i18n: %s, %d of %d strings translated", code, ret, PW_S_COUNT);
}

void pw_lang_init(void)
{
    char dir[200], code[CODE_MAX];
    const char *c;
    int lang;

    plugin_dir(dir, sizeof(dir));
    g_chosen[0] = 0;
    if (read_override(dir, code)) {
        strcpy(g_chosen, code);
        pw_log("i18n: language.txt asks for \"%s\"", code);
        load(dir, code);
        return;
    }
    c = system_code(&lang);
    pw_log("i18n: system language %d", lang);
    load(dir, c);
}

const char *pw_lang_chosen(void)
{
    return g_chosen[0] ? g_chosen : 0;
}

int pw_lang_choose(const char *code)
{
    char dir[200], path[PATH_MAX_];
    int ret = 0, lang;

    plugin_dir(dir, sizeof(dir));
    stbsp_snprintf(path, sizeof(path), "%slanguage.txt", dir);
    if (code == 0) {
        if (!pw_log_card_blocked())
            pw_file_remove(path); /* may not exist */
        g_chosen[0] = 0;
        load(dir, system_code(&lang));
        return pw_log_card_blocked() ? -1 : 0;
    }
    if (pw_log_card_blocked() ||
        pw_file_write(path, code, (unsigned int)strlen(code), 0) != (int)strlen(code)) {
        pw_log("i18n: %s could not be written: \"%s\" for this game only", path, code);
        ret = -1;
    }
    strncpy(g_chosen, code, sizeof(g_chosen) - 1);
    g_chosen[sizeof(g_chosen) - 1] = 0;
    load(dir, code);
    return ret;
}

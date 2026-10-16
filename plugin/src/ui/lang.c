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

static int covers(const char *s, void *ctx)
{
    (void)ctx;
    return pw_text_covers(pw_assets_font(PW_FONT_TITLE), s) &&
           pw_text_covers(pw_assets_font(PW_FONT_BODY), s);
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

void pw_lang_init(void)
{
    char dir[200], path[PATH_MAX_], code[CODE_MAX];
    pw_mem_block mem;
    const char *c = 0;
    int lang = 1, button = 1, size, n, ret;

    pw_i18n_reset();
    if (pw_log_dir(dir, sizeof(dir)) < 0)
        strcpy(dir, "ms0:/seplugins/pocketwatch/");
    if (read_override(dir, code)) {
        c = strcmp(code, "en") == 0 ? 0 : code;
        pw_log("i18n: language.txt asks for \"%s\"", code);
    } else if (sceImposeGetLanguageMode(&lang, &button) >= 0) {
        c = pw_i18n_code(lang);
    }
    if (c == 0) {
        pw_log("i18n: English (system language %d)", lang);
        return;
    }
    stbsp_snprintf(path, sizeof(path), "%slang/%s.txt", dir, c);
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
        pw_log("i18n: %s, %d of %d strings translated", c, ret, PW_S_COUNT);
}

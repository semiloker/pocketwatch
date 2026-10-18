// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdlib.h>
#include <string.h>

#include "hw.h"
#include "texts.h"
#include "ui/i18n.h"

#define DIR      "ms0:/seplugins/pocketwatch/"
#define LANG_MAX (12 * 1024)
#define CODE_MAX 12

/* language.txt's code (as ui/lang.c reads it), or the system's language. */
void hm_texts_load(void)
{
    char code[CODE_MAX], buf[CODE_MAX + 4], path[96], *text;
    const char *c = 0;
    int n = hm_hw_read_file(DIR "language.txt", buf, sizeof(buf) - 1), j = 0;

    for (int i = 0; i < n && j < CODE_MAX - 1; i++) {
        char ch = buf[i];

        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '-' || ch == '_')
            code[j++] = ch >= 'A' && ch <= 'Z' ? (char)(ch - 'A' + 'a') : ch;
        else if (j > 0)
            break;
    }
    code[j] = 0;
    c = j ? code : pw_i18n_code(hm_hw_language());
    pw_i18n_reset();
    if (!c || strcmp(c, "en") == 0 || !(text = malloc(LANG_MAX + 1)))
        return;
    strcpy(path, DIR "lang/");
    strncat(path, c, CODE_MAX);
    strcat(path, ".txt");
    n = hm_hw_read_file(path, text, LANG_MAX);
    if (n > 0)
        pw_i18n_load(text, (unsigned int)n, 0, 0);
    free(text);
}

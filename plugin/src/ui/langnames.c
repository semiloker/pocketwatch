// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "ui/langnames.h"

const pw_lang_name pw_lang_names[] = {
    {0, 0}, /* the system's: PW_S_SYSTEM_LANGUAGE */
    {"en", "English"},
    {"fr", "Français"},
    {"es", "Español"},
    {"de", "Deutsch"},
    {"it", "Italiano"},
    {"nl", "Nederlands"},
    {"pt", "Português"},
    {"ru", "Русский"},
    {"uk", "Українська"},
};
const int pw_lang_name_count = (int)(sizeof(pw_lang_names) / sizeof(pw_lang_names[0]));

int pw_lang_name_index(const char *code)
{
    for (int i = 1; code && i < pw_lang_name_count; i++)
        if (strcmp(code, pw_lang_names[i].code) == 0)
            return i;
    return 0;
}

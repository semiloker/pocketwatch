// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_UI_LANGNAMES_H
#define PW_UI_LANGNAMES_H

/*
 * The languages the Settings screens offer, each under its own name as the system's language
 * setting lists them; entry 0 is "the system's" (code NULL, named by PW_S_SYSTEM_LANGUAGE). Shared
 * by the hub's Settings and the XMB entry (vsh/). Portable.
 */
typedef struct {
    const char *code, *name;
} pw_lang_name;

extern const pw_lang_name pw_lang_names[];
extern const int pw_lang_name_count;

/* The entry for a language.txt code (NULL: the system's), 0 when it is not listed. */
int pw_lang_name_index(const char *code);

#endif

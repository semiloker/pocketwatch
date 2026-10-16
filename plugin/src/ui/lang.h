// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_LANG_H
#define PW_LANG_H

/*
 * Plugin start, after the fonts: pick the language (language.txt next to the PRX overrides the
 * system's), read lang/<code>.txt and hand it to ui/i18n, which keeps what it takes in kernel RAM.
 */
void pw_lang_init(void);

/* The code language.txt holds ("uk"), or NULL when the system language is used. */
const char *pw_lang_chosen(void);

/* Choose a language from the Settings screen: code NULL = the system's. Writes or removes
   language.txt and switches the texts at once. Returns 0, or < 0 if the file could not be
   written (the choice still applies until the game ends). */
int pw_lang_choose(const char *code);

#endif

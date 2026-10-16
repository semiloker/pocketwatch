// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_LANG_H
#define PW_LANG_H

/*
 * Plugin start, after the fonts: pick the language (language.txt next to the PRX overrides the
 * system's), read lang/<code>.txt and hand it to ui/i18n, which keeps what it takes in kernel RAM.
 */
void pw_lang_init(void);

#endif

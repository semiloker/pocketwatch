// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_VSH_XSETTINGS_H
#define PW_VSH_XSETTINGS_H

/*
 * Pocketwatch's settings as the XMB lists them (VSH-REQ-006): the same rows as the hub's Settings
 * screen, read from and written to the files the game side reads at its start (config.txt,
 * language.txt), with the texts of the translations the game side uses.
 */

#define PWX_DIR "ms0:/seplugins/pocketwatch/"

enum { PWX_ROW_STYLE, PWX_ROW_FAST, PWX_ROW_STATUS, PWX_ROW_LANGUAGE, PWX_ROW_COUNT };

void pwx_settings_load(void); /* config.txt, language.txt and the texts again */

int pwx_row_by_key(const char *key); /* "pw_style" ... -> PWX_ROW_*, or -1 */
const char *pwx_row_key(int row);
const char *pwx_row_label(int row); /* UTF-8, in the user's language */
int pwx_row_count(int row);         /* how many values */
const char *pwx_row_option(int row, int i);
int pwx_row_get(int row);
void pwx_row_set(int row, int value); /* applied and saved at once */

#endif

// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_VSH_XSETTINGS_H
#define PW_VSH_XSETTINGS_H

/*
 * Pocketwatch's pages in the XMB (VSH-REQ-006..008, ADR-024), shown as the XMB's own option lists
 * on System Settings' page: Settings (the rows the game side reads from config.txt /
 * language.txt), Battery and Play Time (read-only rows: each has one value, its reading). Texts
 * come from the translations the game side uses.
 */

#define PWX_DIR "ms0:/seplugins/pocketwatch/"

enum { PWX_PAGE_NONE, PWX_PAGE_SETTINGS, PWX_PAGE_BATTERY, PWX_PAGE_PLAYTIME };

#define PWX_ROWS_MAX 18 /* the most rows a Settings page takes without xmbctrl's patch */

void pwx_texts_load(void);    /* language.txt and the translation, again */
void pwx_page_open(int page); /* config and texts again, the page's rows; NONE: no page */
int pwx_page(void);
const char *pwx_page_title(void);

int pwx_rows(void);
int pwx_row_by_key(const char *key); /* "pw_r03" -> 3, or -1 */
const char *pwx_row_key(int row);
const char *pwx_row_label(int row); /* UTF-8, in the user's language */
int pwx_row_count(int row);         /* how many values */
const char *pwx_row_option(int row, int i);
int pwx_row_get(int row);
void pwx_row_set(int row, int value); /* settings: applied and saved at once; others: nothing */

#endif

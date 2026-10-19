// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_HSETTINGS_H
#define HM_HSETTINGS_H

/*
 * Home's Settings section (HOM-REQ-012, docs/research/m7-xmb-inventory.md): Pocketwatch's own
 * settings (config.txt, language.txt - the same files the XMB's page writes), the system's
 * settings the firmware lets an application read and write (sceUtility system parameters), and
 * the screen and sound ones the HOME screen itself changes (impose, through KUBridge). Every
 * write is read back: a setting the firmware refuses shows its real value again.
 */

enum { HS_HEADER, HS_CHOICE, HS_ACTION };
enum { HS_ACT_NONE, HS_ACT_NICKNAME };

/* Changed something every text depends on (the language): the caller reloads its strings. */
#define HS_TEXTS_CHANGED     1
#define HS_WALLPAPER_CHANGED 2 /* Home's background again */

int hs_count(void);
int hs_kind(int i);
int hs_action(int i);                     /* HS_ACTION rows */
const char *hs_label(int i);              /* in the current language */
void hs_value(int i, char *out, int cap); /* the current value's text ("" for headers) */

/* Left or right on a choice row (dir -1 or 1). Returns HS_TEXTS_CHANGED, 0, or < 0 when the
   firmware refused the change. */
int hs_step(int i, int dir);

/* Home's key sounds on (1) or off, as Settings has them (sounds.txt). */
int hs_sounds(void);

/* Home's wallpaper (512 x 272 pixels as Home draws them): read it (0, or < 0 without one), or
   keep a new one made from the picture at path. */
int hs_wallpaper_load(unsigned int *out);
int hs_wallpaper_set(const char *path, const unsigned int *bg);

/* The nickname (UTF-8) as the system has it, and a new one (HS_ACT_NICKNAME). */
void hs_nickname(char *out, int cap);
int hs_set_nickname(const char *utf8);

/* An on/off row: 1 off, 2 on; 0 for any other row (Modern draws those as switches). */
int hs_toggle(int i);

/* A choice's options, for the XMB's option panel (Classic): how many, each one's text, the
   current one (< 0 unknown), and choosing one (as hs_step returns). */
int hs_options(int i);
void hs_option_text(int i, int o, char *out, int cap);
int hs_current(int i);
int hs_choose(int i, int o);

/* The selectable row nearest to i in direction dir (headers are skipped); i itself if none. */
int hs_next(int i, int dir);

#endif

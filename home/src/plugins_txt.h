// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_PLUGINS_TXT_H
#define HM_PLUGINS_TXT_H

/*
 * ARK-4's plugin list (ms0:/seplugins/PLUGINS.TXT: "<runlevel>, <path>, on|off" a line), for
 * the game overlay's module (ADR-043): Home turns its line on or off, the next game loads it or
 * not. Every other line stays as it was (CRLF too). Portable.
 */

/* The text with path's line set to on or off - added (runlevel UMD, as scripts/deploy.ps1 adds
   the game plugin) when there is none and on is asked. Its length, or < 0 when cap is too small. */
int hm_plugins_set(const char *text, int len, const char *path, int on, char *out, int cap);

/* path's line: 1 on, 0 off, -1 none. */
int hm_plugins_get(const char *text, int len, const char *path);

#endif

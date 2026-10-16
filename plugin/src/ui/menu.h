// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_MENU_H
#define PW_MENU_H

/*
 * Minimal in-game menu (docs/research/m3-menu-e3.md, phase 1). A tap of the NOTE (♪) button
 * freezes the game and shows Save state / Load state / Resume over its dimmed picture; the game
 * sees none of the buttons while it is open. The native XMB-style hub (M3/M4) replaces only the
 * drawing later.
 */
int pw_menu_start(void); /* the thread that watches for the NOTE tap */
void pw_menu_stop(void); /* from module_stop */
int pw_menu_is_open(void);

#endif

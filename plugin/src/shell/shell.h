// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_SHELL_H
#define PW_SHELL_H

/*
 * The in-game shell (docs/subsystems/game-shell.md): owns the HOME button while the game runs
 * (the system HOME screen is switched off with sceImposeSetHomePopup), opens the hub on a HOME
 * press, freezes the game, hides the buttons from it, gives everything back on close, and quits
 * the game when the hub asks. When someone else already owns HOME, a tap of NOTE opens the hub.
 */
int pw_shell_start(void);
void pw_shell_stop(void); /* from module_stop: gives HOME back to the system */
int pw_shell_is_open(void);

#endif

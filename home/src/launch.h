// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_LAUNCH_H
#define HM_LAUNCH_H

/* Start a game image the way ARK-4's own menu does (extras/menus/arkMenu/src/iso.cpp,
   Iso::executeISO): Inferno mounts the image, the game's EBOOT starts with apitype 0x123.
   Returns only on failure (< 0). */
int hm_launch(const char *image, int patched);

/* Start an EBOOT.PBP as ARK-4's menu does (Eboot::executeHomebrew / executePOPS): homebrew with
   apitype 0x141, a PS1 game with 0x144. Returns only on failure (< 0). */
int hm_launch_eboot(const char *eboot, int ps1);

/* A PSN game's EBOOT.PBP as ARK-4's menu does (Eboot::executePSN): the np9660 driver, the
   disc's EBOOT.BIN (PBOOT.PBP next to it when there is one). Returns only on failure. */
int hm_launch_psn(const char *eboot);

#endif

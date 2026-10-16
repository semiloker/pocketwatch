// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_SETTINGS_H
#define PW_SETTINGS_H

#include "ui/ui.h"

/*
 * The hub's Settings screen (CPL-REQ-006): fast save and load, the clock and battery corner, the
 * language. Changes apply at once and are written to config.txt / language.txt when it closes.
 * Runs its own loop; returns when the user goes back.
 */
void pw_settings_run(pw_ui *ui);

/* Plugin start: read config.txt and apply it. */
void pw_settings_init(void);

#endif

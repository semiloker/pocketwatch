// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_SLOTS_H
#define PW_SLOTS_H

#include "ui/ui.h"

/*
 * The state list (M5): pick a slot to save to or to load from, delete with triangle, in the
 * Saved Data Utility's layout (docs/ui/README.md UI-REQ-009). Runs its own loop.
 */

enum { PW_SLOTS_SAVE, PW_SLOTS_LOAD };

/* Returns the chosen slot (0-based) or -1 for Back. start: the slot to put the cursor on. */
int pw_slots_pick(pw_ui *ui, int mode, int start);

#endif

// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_HUB_H
#define PW_HUB_H

#include "complications/complication.h"
#include "core/freeze.h"

/*
 * The in-game hub (docs/ui/README.md, concept A): the XMB's vertical item list over the frozen,
 * dimmed game. The game shell opens the framebuffer and freezes the game before, and closes and
 * thaws after.
 */

enum {
    PW_HUB_CLOSE,  /* back to the game */
    PW_HUB_QUIT,   /* the user confirmed Quit Game */
    PW_HUB_LOADED, /* a state was loaded: the screen is gone, leave at once */
};

int pw_hub_run(const pw_freeze_set *frozen);

/* A complication's start-up action (pw_complication.startup) on the open screen. PW_HUB_*. */
int pw_hub_startup(const pw_freeze_set *frozen, const pw_complication *cpl);

#endif

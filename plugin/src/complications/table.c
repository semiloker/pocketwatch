// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Every complication, in the order of its rows in the hub (CPL-REQ-001: adding one is its folder
 * plus one line here). init runs in this order at plugin start, shutdown in reverse.
 */
#include "complications/registry.h"

extern const pw_complication pw_cpl_resume, pw_cpl_states, pw_cpl_playtime, pw_cpl_settings,
    pw_cpl_quit;
#ifdef PW_DEBUG
extern const pw_complication pw_cpl_cardtest;
#endif

const pw_complication *const pw_complications[] = {
    &pw_cpl_resume,   /* complications/system: Resume Game */
    &pw_cpl_states,   /* complications/states: Save State, Load State */
    &pw_cpl_playtime, /* complications/playtime: counts play time (its page: XMB) */
    &pw_cpl_settings, /* complications/settings: reads config.txt (the page: XMB) */
    &pw_cpl_quit,     /* complications/system: Quit Game */
#ifdef PW_DEBUG
    &pw_cpl_cardtest, /* complications/states, debug builds: the T1 Memory Stick test */
#endif
};

const int pw_complication_count = (int)(sizeof(pw_complications) / sizeof(pw_complications[0]));

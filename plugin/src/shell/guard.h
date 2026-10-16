// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_GUARD_H
#define PW_GUARD_H

/*
 * When a save or load must not run (M3b, SHL-REQ-007..010): the hub shows Save State / Load State
 * disabled with the reason, and Enter on them explains it. Checked when the hub opens and every
 * second while it is open.
 */

enum {
    PW_GUARD_OK = 0,
    PW_GUARD_BATTERY, /* below PW_GUARD_BATTERY_MIN % and no AC adaptor */
    PW_GUARD_USB,     /* USB mass storage is active (the PC may write the card) */
    PW_GUARD_WLAN,    /* the wireless LAN is powered (a suspend drops the connection) */
    PW_GUARD_PLUGIN,  /* Custom Home or PSPStates is loaded (they hook HOME and the suspend) */
    PW_GUARD_HIGHMEM, /* game RAM is not the standard 24 MB (ARK's highmem): the engine refuses */
};

#define PW_GUARD_BATTERY_MIN 15

int pw_guard_states(void);
const char *pw_guard_reason(int guard); /* the dialog's first line */
const char *pw_guard_hint(int guard);   /* what to do, the second line */

#endif

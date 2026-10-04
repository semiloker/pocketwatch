// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_ENGINE_H
#define PW_ENGINE_H

/*
 * Savestate engine, in-RAM edition (milestone M2 / rung E2: load within the same boot).
 *
 * SAVE: freeze the game's threads, compress game RAM (P2), eDRAM and the scratchpad into the
 *       PSP-3000's spare RAM (P9 + P11), then take a real system suspend and copy the 4 MB kernel
 *       image at PHASE0_0. An RTC alarm wakes the PSP 100 ms later; the game is thawed.
 * LOAD: freeze the game, take a real suspend and at PHASE0_0 copy the saved kernel image back
 *       (handoff.S). Execution continues inside the SAVE's suspend, whose resume then restores
 *       RAM, eDRAM and scratchpad, re-initialises the flash drivers and thaws the game.
 *       pw_engine_load therefore never returns on success: pw_engine_save returns a second time,
 *       with PW_ENGINE_LOADED.
 *
 * While a state is held no Memory Stick write may happen (log lines stay in RAM): the restored
 * kernel's file-system view must match the card. pw_engine_discard ends that period.
 */

enum {
    PW_ENGINE_SAVED = 0,  /* pw_engine_save: state captured and held in RAM */
    PW_ENGINE_LOADED = 1, /* pw_engine_save: we are back from a pw_engine_load */
};

int pw_engine_init(void);      /* registers the sysevent handler */
void pw_engine_shutdown(void); /* from module_stop: no I/O, just unregister and free */

int pw_engine_save(void);
int pw_engine_load(void); /* returns only on failure (< 0) */

int pw_engine_has_state(void);
void pw_engine_discard(void); /* drop the state, flush the held log lines */

#endif

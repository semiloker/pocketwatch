// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_ENGINE_H
#define PW_ENGINE_H

#include "core/freeze.h"

/*
 * Savestate engine, in-RAM edition (milestone M2 / rung E2: load within the same boot).
 *
 * SAVE: freeze the game's threads, compress game RAM (P2), eDRAM and the scratchpad into the
 *       PSP-3000's spare RAM (P9; P11 is the firmware's ME eDRAM save area during a suspend),
 *       then take a real system suspend and copy the 4 MB kernel image at PHASE0_0. An RTC alarm
 *       wakes the PSP 100 ms later; the game is thawed.
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
    PW_ENGINE_BUSY = -2,  /* another save or load is running */
};

#define PW_ENGINE_USER_WORDS 64

int pw_engine_init(void);      /* registers the sysevent handler */
void pw_engine_shutdown(void); /* from module_stop: no I/O, just unregister and free */

int pw_engine_save(void);
int pw_engine_load(void); /* returns only on failure (< 0) */

/*
 * The same for a caller that has frozen the game itself (the menu): the engine neither freezes
 * nor thaws. After a load, pw_engine_save_held returns PW_ENGINE_LOADED a second time in the
 * caller's save-time context, with the game still frozen: the caller thaws it.
 */
int pw_engine_save_held(const pw_freeze_set *held);
int pw_engine_load_held(const pw_freeze_set *held);

int pw_engine_has_state(void);
int pw_engine_busy(void);
/* System time (ms, sceKernelGetSystemTimeLow / 1000) of the last resume from any suspend. */
unsigned int pw_engine_last_resume_ms(void);

/* Called in the operating thread right before each suspend request (no I/O allowed in it): the
   menu gives the display back there, since P11 is overwritten during the suspend. */
void pw_engine_set_presuspend(void (*fn)(void));

/*
 * Called once after the next rollback, game still frozen, before the engine writes anything to the
 * card; `user` is PW_ENGINE_USER_WORDS words in P9 that survive the rollback (the caller fills
 * them after the save through pw_engine_user_area). A negative return blocks every card write
 * until reboot (pw_log_set_card_blocked). Set it before the save: the hook pointer is part of the
 * snapshot.
 */
void pw_engine_set_load_hook(int (*fn)(unsigned int *user, unsigned int words));
unsigned int *pw_engine_user_area(void); /* NULL without a staged state */

/* Before quitting the game: make sure the IPL seeds are in place, drop the state, flush the log. */
void pw_engine_prepare_exit(void);

/* Debug experiments (see engine.c): rollback mechanics alone; save + immediate frozen load. */
int pw_engine_selftest(void);
int pw_engine_save_load_frozen(void);
void pw_engine_discard(void); /* drop the state, flush the held log lines */

/* Debug: "fast mode" suspends touch-and-go (every suspend/resume event, no real sleep);
   pw_engine_tg_test does one such suspend with nothing else. */
void pw_engine_set_fast(int on);
int pw_engine_fast(void);
int pw_engine_tg_test(void);

#endif

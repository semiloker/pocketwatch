// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_FREEZE_H
#define PW_FREEZE_H

/*
 * Freezing the running game: suspend every user-mode thread (attribute 0x80000000; the game's,
 * see docs/research/m1-hardware.md). System, ARK and plugin threads are kernel threads and keep
 * running. A thread is preferably suspended while it is WAITING, i.e. parked at a safe point;
 * after the timeout the stragglers are suspended anyway. WAITING is not always safe: a thread
 * waiting inside a read of the card holds the FAT driver, so the card must answer after the
 * freeze, or the game runs again until it does (gives up after 2 s: card_busy, nothing frozen).
 */

#define PW_FREEZE_MAX 64

typedef struct {
    int ids[PW_FREEZE_MAX];
    int count;       /* threads we suspended */
    int forced;      /* of those, suspended while not WAITING (timeout) */
    unsigned int ms; /* time taken */
    int card_tries;  /* freezes undone because a frozen thread held the card */
    int card_busy;   /* 1: the card never came free, nothing is frozen */
} pw_freeze_set;

/* Returns the number of threads frozen, or < 0 on error. Log details with pw_freeze_log. */
int pw_freeze_game(pw_freeze_set *fs, unsigned int timeout_ms);

/* Resume exactly the threads pw_freeze_game suspended, in the same order. */
void pw_thaw_game(pw_freeze_set *fs);

void pw_freeze_log(const pw_freeze_set *fs);

#endif

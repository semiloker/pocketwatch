// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_FREEZE_H
#define PW_FREEZE_H

/*
 * Freezing the running game: suspend every user-mode thread (attribute 0x80000000; the game's,
 * see docs/research/m1-hardware.md). System, ARK and plugin threads are kernel threads and keep
 * running. A thread is preferably suspended while it is WAITING, i.e. parked at a safe point and
 * not in the middle of a kernel call; after the timeout the stragglers are suspended anyway.
 */

#define PW_FREEZE_MAX 64

typedef struct {
    int ids[PW_FREEZE_MAX];
    int count;       /* threads we suspended */
    int forced;      /* of those, suspended while not WAITING (timeout) */
    unsigned int ms; /* time taken */
} pw_freeze_set;

/* Returns the number of threads frozen, or < 0 on error. Log details with pw_freeze_log. */
int pw_freeze_game(pw_freeze_set *fs, unsigned int timeout_ms);

/* Resume exactly the threads pw_freeze_game suspended, in the same order. */
void pw_thaw_game(pw_freeze_set *fs);

void pw_freeze_log(const pw_freeze_set *fs);

#endif

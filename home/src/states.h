// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_STATES_H
#define HM_STATES_H

/*
 * A game's save states as Home shows them (HOM-REQ-015): the slots Pocketwatch wrote for it
 * (ms0:/PSP/SAVEDATA/PW<disc ID><01-05>, docs/subsystems/states-storage.md) with their picture and
 * time, and "continue from this one": continue.txt for the game plugin (M7 V1), then the game.
 */

#define HM_STATES_MAX 5

typedef struct {
    int slot;           /* 1-5 */
    char when[32];      /* the time it was saved, in the system's format */
    unsigned int *icon; /* the state's ICON0 (HM_ICON_W x HM_ICON_H), or NULL */
} hm_state;

/* The states of the game with this disc ID, newest first. Returns how many. */
int hm_states_list(const char *disc_id, hm_state *out);
void hm_states_free(hm_state *s, int n);

/* Ask the game plugin to load that slot once the game runs. 0 when written. */
int hm_states_continue(const char *disc_id, int slot);

#endif

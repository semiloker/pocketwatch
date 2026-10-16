// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_STORE_H
#define PW_STORE_H

#include "core/platform.h"

/*
 * States on the Memory Stick as native saved data (E3 + M5, docs/subsystems/states-storage.md):
 * slot N of game G is ms0:/PSP/SAVEDATA/PW<G><NN>/ with a hashed PARAM.SFO (so the XMB's Saved Data
 * Utility lists and deletes it), ICON0.PNG and PIC1.PNG from the game's frame, DATA.BIN (the
 * engine's state header, kernel image, packed RAM) and STATE.BIN (a small header written last: a
 * slot without a valid STATE.BIN is empty).
 */

#define PW_STORE_SLOTS  5
#define PW_STORE_ICON_W 144
#define PW_STORE_ICON_H 80

enum {
    PW_STORE_OK = 0,
    PW_STORE_E_NO_GAME = -20,     /* the game could not be identified */
    PW_STORE_E_IO = -21,          /* the Memory Stick refused a read or write (full? removed?) */
    PW_STORE_E_EMPTY = -22,       /* nothing (valid) in that slot */
    PW_STORE_E_OTHER_GAME = -23,  /* saved from another game or ISO */
    PW_STORE_E_OTHER_SETUP = -24, /* another Pocketwatch build, model or memory layout */
    PW_STORE_E_DAMAGED = -25,     /* the files do not match their checksums */
    PW_STORE_E_NO_STATE = -26,    /* nothing to write */
    PW_STORE_E_MEMORY = -27,
    PW_STORE_E_BLOCKED = -28, /* card writes stopped until reboot (a check after a load failed) */
};

typedef struct {
    int exists;
    pw_datetime saved;  /* local time of the save */
    unsigned int size;  /* bytes on the card */
    int has_icon;       /* icon filled from ICON0.PNG */
    unsigned int *icon; /* PW_STORE_ICON_W x PW_STORE_ICON_H pixels, owned by the caller */
} pw_slot_info;

/* Plugin start, after pw_game_init: prepares the slots' PARAM.SFO (hashed by chnnlsv). */
int pw_store_init(void);
void pw_store_shutdown(void);
int pw_store_available(void);

/* Look at every slot. icons: PW_STORE_SLOTS buffers of 144 x 80 pixels, or NULL for no icons. */
void pw_store_scan(pw_slot_info *slots, unsigned int *icons);

/* Progress of a long write or read: percent done. */
typedef void (*pw_store_progress)(int percent, void *ctx);

/* Write the engine's held state to slot (0-based), with the game's frame (480x272, stride
   pixels per row) for ICON0/PIC1. */
int pw_store_save(int slot, const unsigned int *frame, int stride, pw_store_progress cb, void *ctx);

/* Read slot into the engine and check it (pw_engine_adopt); the caller then loads. */
int pw_store_load(int slot, pw_store_progress cb, void *ctx);

int pw_store_delete(int slot);

/* Slot whose content the engine holds in RAM right now, or -1. */
int pw_store_held_slot(void);

/*
 * The check after a load (STS-REQ-008). Every save to a slot creates a marker file with a new name
 * after the kernel capture; a restored kernel with a stale view of the card cannot see it.
 * pw_store_arm(slot) right before a load (slot < 0: a state that is not on the card, no check);
 * pw_store_after_load is the engine's load hook (pw_engine_set_load_hook), set before every save
 * so that it is part of the snapshot. pw_store_check_failed tells the restored world the result.
 */
void pw_store_arm(int slot);
int pw_store_after_load(unsigned int *user, unsigned int words);
int pw_store_check_failed(void);

/* A short reason for a PW_STORE_E_* / PW_ENGINE_E_* code, for the message dialog. */
const char *pw_store_reason(int err);

#endif

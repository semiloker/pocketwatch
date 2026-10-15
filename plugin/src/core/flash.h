// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_FLASH_H
#define PW_FLASH_H

/*
 * The flash translation layer (lflash: logical -> physical NAND block map, shared by flash0: and
 * flash1:) and the FAT driver on top (flashfat) keep their state in kernel RAM. A savestate load
 * rolls that state back to the save moment; if the NAND was written in between, the next flash1:
 * write (registry flush, at the latest when the game quits) goes through a stale map and corrupts
 * flash1, and the PSP resets its settings at the next boot (E2 run 5).
 *
 * The cure is PSPFatSave's (MIT; its v512-v520 history documents the same symptom): call both
 * drivers' own df_exit + df_init in place, which rescans the NAND and rebuilds the map, after every
 * resume; after a rollback also unassign/reassign flash0:/flash1: around it so both mounts are
 * rebuilt on the fresh map. No sceIoDelDrv: that would kill the game's open flash0: handles.
 *
 * Called from the RESUME_COMPLETED sysevent, before other threads touch flash: no logging, no
 * big stack frames. The outcome is returned in a report for the caller to log later.
 */

typedef struct {
    int remount;
    int unassign0, unassign1;
    int exit_lflash, exit_flashfat, init_lflash, init_flashfat;
    int assign0, assign1;
    int found_lflash, found_flashfat;
} pw_flash_report;

/* 1 when both drivers can be found, i.e. pw_flash_reinit can work. Call before a load. */
int pw_flash_available(void);

/* Returns 0 when both drivers were re-initialised (and, with remount, both devices reassigned). */
int pw_flash_reinit(pw_flash_report *r, int remount);

#endif

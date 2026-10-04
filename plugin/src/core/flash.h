// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_FLASH_H
#define PW_FLASH_H

/*
 * After a savestate load the kernel's flash drivers (lflash, flashfat) hold state from the save
 * moment. Re-initialise them in place and reassign flash0:/flash1:, so the system settings
 * (registry on flash1) are not written back from a stale cache. Same technique as PSPFatSave
 * (MIT) and ARK-4's driver lookup.
 *
 * Called from the RESUME_COMPLETED sysevent, before other threads touch flash: no logging, no
 * big stack frames. The outcome is returned in a report for the caller to log later.
 */

typedef struct {
    int unassign0, unassign1;
    int exit_lflash, exit_flashfat, init_lflash, init_flashfat;
    int assign0, assign1;
    int found_lflash, found_flashfat;
} pw_flash_report;

/* Returns 0 when both drivers were found and both devices reassigned, else -1. */
int pw_flash_reinit(pw_flash_report *r);

#endif

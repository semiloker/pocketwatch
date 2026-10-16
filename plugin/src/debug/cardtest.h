// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_CARDTEST_H
#define PW_CARDTEST_H

/*
 * Test T1 (docs/research/m3-menu-e3.md, docs/subsystems/states-storage.md STS-REQ-008): does the
 * Memory Stick driver see the card as it is after a rollback, or as it was at the save? Debug
 * builds only, run from the hub:
 *   1. pw_cardtest_prepare   before the save: set the engine's load hook
 *   2. (save)
 *   3. pw_cardtest_write_probe  write probe file A (a new file, new clusters) after the save
 *   4. (load) -> after the rollback the hook, before any other card write: invalidate ARK's
 *      Memory Stick cache, look for A and verify its contents; only if that passed, write probe
 *      B and read both back. A failure blocks every card write until reboot.
 *   5. pw_cardtest_result   in the hub, back in the saved moment
 */

int pw_cardtest_prepare(void);
int pw_cardtest_write_probe(void);
void pw_cardtest_cancel(void);
void pw_cardtest_result(char *line1, char *line2, unsigned int cap);

#endif

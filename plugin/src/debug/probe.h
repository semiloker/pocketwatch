// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_PROBE_H
#define PW_PROBE_H

/*
 * Development probe (debug builds only): writes a map of the running system to the log so the
 * savestate engine is designed against real PSP-3000 data, not guesses (milestone M1 / spike S1).
 *
 * pw_probe_boot  - short snapshot at plugin start: model, firmware, game, memory partitions.
 * pw_probe_start - starts a thread that, during gameplay,
 *                  on L + R + SELECT + UP   dumps partitions, modules and threads;
 *                  on L + R + SELECT + LEFT runs spike S2b (debug/spike_savedata.h).
 *                  on L + R + SELECT + RIGHT runs spike E1 (debug/spike_suspend.h).
 * pw_probe_stop  - stops and deletes that thread; must be called from module_stop.
 */
void pw_probe_boot(void);
void pw_probe_start(void);
void pw_probe_stop(void);

#endif

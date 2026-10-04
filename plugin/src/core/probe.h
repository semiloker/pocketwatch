// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_PROBE_H
#define PW_PROBE_H

/*
 * Development probe (debug builds only): writes a map of the running system to the log so the
 * savestate engine is designed against real PSP-3000 data, not guesses (milestone M1 / spike S1).
 *
 * pw_probe_boot  - short snapshot at plugin start: model, firmware, game, memory partitions.
 * pw_probe_start - starts a thread that dumps partitions, modules and threads whenever
 *                  L + R + SELECT + UP is held (do it during gameplay).
 */
void pw_probe_boot(void);
void pw_probe_start(void);

#endif

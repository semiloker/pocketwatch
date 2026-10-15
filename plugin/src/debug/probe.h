// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_PROBE_H
#define PW_PROBE_H

/*
 * Development probe (debug builds only): writes a map of the running system to the log so the
 * savestate engine is designed against real PSP-3000 data, not guesses (milestone M1 / spike S1).
 *
 * pw_probe_boot  - short snapshot at plugin start: model, firmware, game, memory partitions.
 * pw_probe_start - starts a thread that, during gameplay,
 *                  on L + R + START + RIGHT  saves a state to RAM (engine.h);
 *                  on L + R + START + LEFT   loads it back;
 *                  on L + R + START + UP     runs the throughput benchmark (bench.h);
 *                  on L + R + SELECT + UP    drops a held state (flushing the log) and dumps
 *                                            partitions, modules and threads;
 *                  on L + R + SELECT + RIGHT runs the rollback self-test;
 *                  on L + R + SELECT + LEFT  saves and loads at once, game kept frozen;
 *                  on L + R + SELECT + TRIANGLE one touch-and-go suspend (no sleep), nothing else;
 *                  on L + R + SELECT + SQUARE   toggles touch-and-go ("fast mode") for the above.
 * pw_probe_stop  - stops and deletes that thread; must be called from module_stop.
 */
void pw_probe_boot(void);
void pw_probe_start(void);
void pw_probe_stop(void);

#endif

// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_SPIKE_SAVEDATA_H
#define PW_SPIKE_SAVEDATA_H

/*
 * Spike S2b (debug builds, L + R + SELECT + LEFT): can Pocketwatch write save folders that the
 * XMB accepts?
 *  1. Re-hash ms0:/PSP/SAVEDATA/ARK_01234/PARAM.SFO and compare with its stored hash: proves the
 *     chnnlsv-based hashing reproduces the firmware's result.
 *  2. Create PWTEST00C (empty file list) and PWTEST00D (STATE.BIN listed) with hashed SFOs and
 *     PNGs from our encoder, timing each step.
 * Results go to the log; then check Game -> Saved Data Utility.
 */
void pw_spike_savedata(void);

#endif

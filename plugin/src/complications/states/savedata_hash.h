// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_SAVEDATA_HASH_H
#define PW_SAVEDATA_HASH_H

/*
 * PARAM.SFO hashing so the XMB accepts our save folders (hash-less ones show as
 * "Corrupted Data", see docs/research/m1-hardware.md). Uses the firmware's own chnnlsv.prx
 * (loaded on demand), so no keys live in Pocketwatch. Algorithm as in PPSSPP
 * Core/Dialog/SavedataParam.cpp (UpdateHash/BuildHash), protection mode 0x01: console independent.
 */

/* Load chnnlsv.prx if needed and resolve its MAC functions. Returns 0 or < 0. */
int pw_savedata_hash_init(void);

/*
 * Rewrite SAVEDATA_PARAMS of `sfo` (size must be a multiple of 16) with mode 0x01 hashes.
 * params_offset as returned by pw_sfo_build_savedata. Returns 0 or < 0.
 */
int pw_savedata_hash_sfo(unsigned char *sfo, unsigned int size, unsigned int params_offset);

#endif

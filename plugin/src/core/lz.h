// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_LZ_H
#define PW_LZ_H

/*
 * LZ4 block codec (lib/lz4 1.10.0, BSD-2-Clause), built freestanding with an 8 KB hash table that
 * fits the Allegrex's 16 KB D-cache. Chosen by the on-device benchmark (docs/research/m2-speed.md):
 * on 24 MB of game RAM it packs ~2.7x faster than FastLZ and unpacks ~1.7x faster, to a slightly
 * smaller size.
 */

/* Bytes of caller-provided state pw_lz_pack needs (pointer-aligned). */
unsigned int pw_lz_state_size(void);

/* Compress len bytes into dst (cap bytes). Returns the packed size, or 0 when it does not fit. */
int pw_lz_pack(const void *src, int len, void *dst, int cap, void *state);

/* Decompress exactly into dst (cap bytes). Returns the unpacked size, or < 0 on corrupt input. */
int pw_lz_unpack(const void *src, int len, void *dst, int cap);

#endif

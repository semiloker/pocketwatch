// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_CODEC_LZ4_H
#define PW_CODEC_LZ4_H

/* LZ4 block codec (benchmark candidate). state: pw_lz4_state_size() bytes, 8-byte aligned. */
unsigned int pw_lz4_state_size(void);
/* Returns the packed size, or 0 when it does not fit cap. */
int pw_lz4_pack(const void *src, int len, void *dst, int cap, void *state, int accel);
/* Returns the unpacked size, or < 0 on corrupt input. */
int pw_lz4_unpack(const void *src, int len, void *dst, int cap);

#endif

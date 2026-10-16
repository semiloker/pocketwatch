// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_RLZ_H
#define PW_RLZ_H

/*
 * Decoder for Sony's RLZ compression (RCO tables and images in flash0:/vsh/resource, compression
 * type 2). Written as the mirror of the RLZ encoder in RCOMage (rlzpack.c, LGPL-2.1, ZiNgA BuRgA)
 * with the PSP's own probability update; verified against stock 6.60 RCO files
 * (docs/research/m4-native-icons.md). Portable and bounds-checked.
 */

#define PW_RLZ_STATE 3712 /* bytes of probability state the caller provides */

/* Returns the number of bytes written to dst, or < 0 for damaged input / too small a dst. */
int pw_rlz_decode(const void *src, unsigned int src_len, void *dst, unsigned int cap,
                  unsigned char *state);

#endif

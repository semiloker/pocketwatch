// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_CRC32_H
#define PW_CRC32_H

/* CRC-32 as used by PNG and zlib (reflected polynomial 0xEDB88320). Start with crc = 0. */
unsigned int pw_crc32(unsigned int crc, const void *data, unsigned int len);

/*
 * Rotate-and-add checksum over 32-bit words (len / 4 of them; data 4-byte aligned). About three
 * cycles per word on the PSP: meant for "was this memory overwritten", not for transfer errors.
 */
unsigned int pw_sum32(const void *data, unsigned int len);

#endif

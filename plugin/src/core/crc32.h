// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_CRC32_H
#define PW_CRC32_H

/* CRC-32 as used by PNG and zlib (reflected polynomial 0xEDB88320). Start with crc = 0. */
unsigned int pw_crc32(unsigned int crc, const void *data, unsigned int len);

#endif

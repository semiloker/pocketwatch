// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_BENCH_H
#define PW_BENCH_H

/*
 * Debug benchmark (L + R + START + UP in a game): measures the building blocks of save and load on
 * the frozen game's real RAM — copies, checksums, zero pages, compression and decompression — and
 * logs MB/s and ratios. No suspend happens and nothing is kept: safe to run any time no state is
 * held. Lets compressor and copy variants be compared on the device in seconds.
 */
int pw_bench_run(void);

#endif

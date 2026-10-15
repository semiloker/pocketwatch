// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * The engine's FastLZ (lib/fastlz, MIT) compiled a second time with -O2 under other names, so the
 * benchmark can show what the optimisation level alone is worth (the engine copy is built -Os).
 */
#define fastlz_compress       pw_fastlz_o2_compress
#define fastlz_decompress     pw_fastlz_o2_decompress
#define fastlz_compress_level pw_fastlz_o2_compress_level
#define fastlz_htab           pw_fastlz_o2_htab
#include "fastlz/fastlz.c"

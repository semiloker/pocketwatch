// SPDX-License-Identifier: MIT OR Unlicense
// Single compilation unit for stb_sprintf (vendored from pspdev, v1.10).
// NOFLOAT keeps the FPU out of kernel code; NOUNALIGNED because Allegrex traps on unaligned loads.
#define STB_SPRINTF_IMPLEMENTATION
#define STB_SPRINTF_NOFLOAT
#define STB_SPRINTF_NOUNALIGNED
#include "stb_sprintf.h"

// SPDX-License-Identifier: GPL-3.0-or-later
// Import stubs for the ARK-4 SystemControl kernel API we use (see core/sysctrl.h).
// NIDs are SHA-1 based (first 4 bytes, little endian) and checked by `make check`.

	.set noreorder

#include "pspimport.s"

	IMPORT_START "SystemCtrlForKernel",0x00090000
	IMPORT_FUNC  "SystemCtrlForKernel",0x159AF5CC,sctrlHENFindFunction

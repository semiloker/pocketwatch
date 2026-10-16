// SPDX-License-Identifier: GPL-3.0-or-later
// LoadCoreForKernel imports: finding a loaded module by name (is a system dialog up?). NID
// resolved by ARK-4 on 6.61 and checked by `make check`.

	.set noreorder

#include "pspimport.s"

	IMPORT_START "LoadCoreForKernel",0x00010000
	IMPORT_FUNC  "LoadCoreForKernel",0xCF8A41B1,sceKernelFindModuleByName

// SPDX-License-Identifier: GPL-3.0-or-later
// LoadExecForKernel imports for Quit Game: the game's own exit callback, like the system's HOME
// screen (uOFW loadexec.c). NIDs resolved by ARK-4 on 6.61 and checked by `make check`.

	.set noreorder

#include "pspimport.s"

	IMPORT_START "LoadExecForKernel",0x00010000
	IMPORT_FUNC  "LoadExecForKernel",0x659188E1,sceKernelCheckExitCallback
	IMPORT_FUNC  "LoadExecForKernel",0x62A27008,sceKernelInvokeExitCallback

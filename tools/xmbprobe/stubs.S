// SPDX-License-Identifier: GPL-3.0-or-later
// Imports of xmbprobe: ARK-4's user API, the NIDs pocketwatch_xmb.prx imports (vsh/src/stubs.S).

	.set noreorder

#include "pspimport.s"

	IMPORT_START "SystemCtrlForUser",0x40090000
	IMPORT_FUNC  "SystemCtrlForUser",0x1C90BECB,sctrlHENSetStartModuleHandler
	IMPORT_FUNC  "SystemCtrlForUser",0x17691875,sctrlFlushCache

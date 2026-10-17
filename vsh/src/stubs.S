// SPDX-License-Identifier: GPL-3.0-or-later
// Imports of the XMB plugin.
// scePaf: the VSH's user library, NIDs as ARK-4's xmbctrl imports them on 6.61
// (extras/modules/xmbctrl/stub.S). The first three are only compared against, to find the same
// imports in vsh_module and sysconf_plugin; malloc makes the rows the Settings page frees.
// SystemCtrlForUser: ARK-4's user API, NIDs from the names (SHA-1, as for SystemCtrlForKernel).

	.set noreorder

#include "pspimport.s"

	IMPORT_START "scePaf",0x00090011
	IMPORT_FUNC  "scePaf",0x3874A5F8,scePafGetText
	IMPORT_FUNC  "scePaf",0x50DCB767,PAF_Resource_GetPageNodeByID
	IMPORT_FUNC  "scePaf",0x64A31513,PAF_Resource_ResolveRefWString
	IMPORT_FUNC  "scePaf",0x5E909060,sce_paf_private_malloc

	IMPORT_START "SystemCtrlForUser",0x40090000
	IMPORT_FUNC  "SystemCtrlForUser",0x1C90BECB,sctrlHENSetStartModuleHandler
	IMPORT_FUNC  "SystemCtrlForUser",0x17691875,sctrlFlushCache

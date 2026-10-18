// SPDX-License-Identifier: GPL-3.0-or-later
// ARK-4's user API for starting a game image (NIDs from ARK-4's libs/SystemCtrlForUser, the
// names' SHA-1 like every SystemCtrl library); KUBridge's kuKernelCall (ARK-4 libs/KUBridge) to
// run a kernel function: the HOME popup and the button peek.

	.set noreorder

#include "pspimport.s"

	IMPORT_START "SystemCtrlForUser",0x40090000
	IMPORT_FUNC  "SystemCtrlForUser",0x2D10FB28,sctrlKernelLoadExecVSHWithApitype
	IMPORT_FUNC  "SystemCtrlForUser",0x5A35C948,sctrlSESetUmdFile
	IMPORT_FUNC  "SystemCtrlForUser",0xBC939DC1,sctrlSESetBootConfFileIndex
	IMPORT_FUNC  "SystemCtrlForUser",0x31C6160D,sctrlSESetDiscType
	IMPORT_FUNC  "SystemCtrlForUser",0x159AF5CC,sctrlHENFindFunction

	IMPORT_START "KUBridge",0x40090000
	IMPORT_FUNC  "KUBridge",0x9060F69D,kuKernelCall
	IMPORT_FUNC  "KUBridge",0x4C25EA72,kuKernelLoadModule

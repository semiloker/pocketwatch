// SPDX-License-Identifier: GPL-3.0-or-later
// ARK-4's user API for starting a game image (NIDs from ARK-4's libs/SystemCtrlForUser, the
// names' SHA-1 like every SystemCtrl library).

	.set noreorder

#include "pspimport.s"

	IMPORT_START "SystemCtrlForUser",0x40090000
	IMPORT_FUNC  "SystemCtrlForUser",0x2D10FB28,sctrlKernelLoadExecVSHWithApitype
	IMPORT_FUNC  "SystemCtrlForUser",0x5A35C948,sctrlSESetUmdFile
	IMPORT_FUNC  "SystemCtrlForUser",0xBC939DC1,sctrlSESetBootConfFileIndex
	IMPORT_FUNC  "SystemCtrlForUser",0x31C6160D,sctrlSESetDiscType

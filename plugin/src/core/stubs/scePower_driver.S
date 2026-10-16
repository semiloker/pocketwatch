// SPDX-License-Identifier: GPL-3.0-or-later
// Every scePower_driver import of the plugin, in one place. PSPSDK's libpsppower_driver is not
// linked: it does not declare the touch-and-go request, and mixing its stubs with ours splits the
// library's stub table (psp-fixup-imports assumes one contiguous run per library). NIDs are the
// 6.60 ones, resolved by ARK-4 on 6.61 and checked by `make check`.

	.set noreorder

#include "pspimport.s"

	IMPORT_START "scePower_driver",0x00010000
	IMPORT_FUNC  "scePower_driver",0xAC32C9CC,scePowerRequestSuspend
	IMPORT_FUNC  "scePower_driver",0xFEE03A2F,scePowerGetCpuClockFrequency
	IMPORT_FUNC  "scePower_driver",0x478FE6F5,scePowerGetBusClockFrequency
	IMPORT_FUNC  "scePower_driver",0x7FA406DD,scePowerIsRequest
	// Suspend "touch and go": every suspend and resume event and handler runs, but syscon never
	// powers the PSP down (uOFW power/powerSwitch.c, 0x2D5C; 6.61 NID 0xD1FFF513 via ARK).
	IMPORT_FUNC  "scePower_driver",0x2875994B,scePowerRequestSuspendTouchAndGo

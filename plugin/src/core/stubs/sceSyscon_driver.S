// SPDX-License-Identifier: GPL-3.0-or-later
// sceSyscon_driver imports (PSPSDK has no library for it): getters of the syscon driver's cached
// switch and device-power state (no syscon traffic), for a touch-and-go resume (engine.c).
// NIDs are the 6.60 ones, resolved by ARK-4 on 6.61 and checked by `make check`.

	.set noreorder

#include "pspimport.s"

	IMPORT_START "sceSyscon_driver",0x00010000
	IMPORT_FUNC  "sceSyscon_driver",0xF6BB4649,sceSysconGetHoldSwitch
	IMPORT_FUNC  "sceSyscon_driver",0xBDA16E46,sceSysconGetWlanSwitch
	IMPORT_FUNC  "sceSyscon_driver",0xE0DDFE18,sceSysconGetHPConnect
	IMPORT_FUNC  "sceSyscon_driver",0x628F2351,sceSysconGetMsPowerCtrl
	IMPORT_FUNC  "sceSyscon_driver",0xEC37C549,sceSysconGetWlanPowerCtrl
	IMPORT_FUNC  "sceSyscon_driver",0x3C739F57,sceSysconGetHRPowerCtrl

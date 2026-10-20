// SPDX-License-Identifier: GPL-3.0-or-later
// Every driver import of the overlay module, in one place (one stub table per library: see
// plugin/src/core/stubs/scePower_driver.S). NIDs are the pre-6.x ones, translated by ARK-4 on 6.61
// (tools/nids/ark4_660.tsv) and checked by `make check`.

	.set noreorder

#include "pspimport.s"

	IMPORT_START "scePower_driver",0x00010000
	IMPORT_FUNC  "scePower_driver",0xFEE03A2F,scePowerGetCpuClockFrequency
	IMPORT_FUNC  "scePower_driver",0x478FE6F5,scePowerGetBusClockFrequency
	IMPORT_FUNC  "scePower_driver",0x2085D15D,scePowerGetBatteryLifePercent
	IMPORT_FUNC  "scePower_driver",0x0AFD0D8B,scePowerIsBatteryExist
	IMPORT_FUNC  "scePower_driver",0x8EFB3FA2,scePowerGetBatteryLifeTime
	IMPORT_FUNC  "scePower_driver",0x28E12023,scePowerGetBatteryTemp
	IMPORT_FUNC  "scePower_driver",0x483CE86B,scePowerGetBatteryVolt

	IMPORT_START "sceSyscon_driver",0x00010000
	IMPORT_FUNC  "sceSyscon_driver",0x373EC933,sceSysconBatteryGetElec

	IMPORT_START "sceDisplay_driver",0x00010000
	IMPORT_FUNC  "sceDisplay_driver",0xEEDA2E54,sceDisplayGetFrameBuf
	IMPORT_FUNC  "sceDisplay_driver",0x984C27E7,sceDisplayWaitVblankStart
	IMPORT_FUNC  "sceDisplay_driver",0x5B5AEFAD,sceDisplayGetFrameBufferInternal

	IMPORT_START "sceCtrl_driver",0x00010000
	IMPORT_FUNC  "sceCtrl_driver",0x3A622550,sceCtrlPeekBufferPositive

	IMPORT_START "SysMemForKernel",0x00010000
	IMPORT_FUNC  "SysMemForKernel",0x9697CD32,sceKernelPartitionTotalFreeMemSize

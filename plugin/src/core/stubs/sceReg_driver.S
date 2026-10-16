// SPDX-License-Identifier: GPL-3.0-or-later
// sceReg_driver imports: reading the system's date/time format and theme colour once at start.
// Read only (no sceRegFlush*, no Set*). NIDs resolved by ARK-4 on 6.61, checked by `make check`.

	.set noreorder

#include "pspimport.s"

	IMPORT_START "sceReg_driver",0x00010000
	IMPORT_FUNC  "sceReg_driver",0x92E41280,sceRegOpenRegistry
	IMPORT_FUNC  "sceReg_driver",0xFA8A5739,sceRegCloseRegistry
	IMPORT_FUNC  "sceReg_driver",0x1D8A762E,sceRegOpenCategory
	IMPORT_FUNC  "sceReg_driver",0x0CAE832B,sceRegCloseCategory
	IMPORT_FUNC  "sceReg_driver",0xD4475AA8,sceRegGetKeyInfo
	IMPORT_FUNC  "sceReg_driver",0x28A8E98A,sceRegGetKeyValue

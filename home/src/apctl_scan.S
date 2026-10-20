// SPDX-License-Identifier: GPL-3.0-or-later
// sceNetApctl's search (Wi-Fi in Home, HOM-REQ-033), past PSPSDK's library (NIDs: PPSSPP
// Core/HLE/sceNet.cpp, sceNetApctl's table). Stubs of the SDK's own table - no IMPORT_START of
// ours: two tables of one library were merged wrongly - linked right after -lpspnet_apctl so they
// follow its stubs (home/Makefile); psp-fixup-imports needs a library's stubs together.

	.set noreorder

#include "pspimport.s"

	IMPORT_FUNC  "sceNetApctl",0xE9B2E5E6,sceNetApctlScanUser
	IMPORT_FUNC  "sceNetApctl",0x6BDDCB8C,sceNetApctlGetBSSDescIDListUser
	IMPORT_FUNC  "sceNetApctl",0x04776994,sceNetApctlGetBSSDescEntryUser

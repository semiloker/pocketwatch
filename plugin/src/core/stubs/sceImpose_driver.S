// SPDX-License-Identifier: GPL-3.0-or-later
// sceImpose_driver imports (the HOME popup and the system's language/button settings). PSPSDK has
// no kernel stub library for impose. NIDs are the pre-6.x ones, resolved by ARK-4 on 6.61
// (tools/nids/ark4_660.tsv) and checked by `make check`.

	.set noreorder

#include "pspimport.s"

	IMPORT_START "sceImpose_driver",0x00010000
	IMPORT_FUNC  "sceImpose_driver",0x5595A71A,sceImposeSetHomePopup
	IMPORT_FUNC  "sceImpose_driver",0x0F341BE4,sceImposeGetHomePopup
	IMPORT_FUNC  "sceImpose_driver",0x24FD7BCF,sceImposeGetLanguageMode

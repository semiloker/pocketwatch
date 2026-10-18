// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_GFX_GU_H
#define HM_GFX_GU_H

#include "gfx.h"

/*
 * The GE backend of hm_gfx (ADR-028): two 8888 frames in VRAM, a display list a frame, every
 * picture a texture. Text, corners and drawn icons are rasterised once by the CPU renderer into
 * small textures and kept (keyed by what they show), so a frame costs the CPU only its list.
 */

int hm_gu_init(void);
hm_gfx *hm_gu_begin(void); /* a new frame: start its list */
/* Finish the list, wait for the GE, show the frame at the next vblank. Returns the GE's time
   for the list in microseconds. */
unsigned int hm_gu_end(void);

/* After the CPU wrote pixels the GE will read (the loader's icons and backgrounds). */
void hm_gu_written(const void *p, unsigned int bytes);

#endif

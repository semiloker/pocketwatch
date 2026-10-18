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
void hm_gu_finish(void);   /* the list is complete: the GE draws it; the CPU may warm stamps */
/* Wait for the GE, show the frame at the next vblank. Returns how long the wait for the GE was
   (microseconds; 0 when it had finished during the warm-up). */
unsigned int hm_gu_end(void);
/* Stamps made this frame, kept, and their bytes (home.log). */
void hm_gu_stats(unsigned int *made, unsigned int *count, unsigned int *bytes);

/* After the CPU wrote pixels the GE will read (the loader's icons and backgrounds). */
void hm_gu_written(const void *p, unsigned int bytes);

/* A finished 480 x 272 background (stride 512, 8888) copied into VRAM slot 0-2 as 5650,
   dithered; drawing that picture then reads VRAM (ADR-030). Any thread. */
void hm_gu_upload_bg(int slot, const unsigned int *px);

#endif

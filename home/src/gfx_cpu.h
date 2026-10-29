// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_GFX_CPU_H
#define HM_GFX_CPU_H

#include "gfx.h"
#include "ui/draw.h"

/* The CPU backend of hm_gfx over a canvas (the host previews). Portable. */
typedef struct {
    hm_gfx ops;
    pw_canvas *c;
    pw_canvas rows; /* a clip: the canvas's rows from oy on (clip()) */
    int oy, clipped;
} hm_gfx_cpu;

void hm_gfx_cpu_init(hm_gfx_cpu *g, pw_canvas *c);

#endif

// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_ANIM_H
#define PW_ANIM_H

/*
 * Motion that depends on time, not on the frame rate: the XMB moves a list or a cursor with an
 * exponential curve whose time constant is a quarter of the move's nominal duration (200 ms by
 * default, so tau = 50 ms; VitaXMB's measurements of the PSP's engine). Portable.
 */

#define PW_ANIM_TAU_MS 50 /* lists, the cursor, glows */

/* Move cur towards target for dt_ms; snaps when within `snap`. */
static inline int pw_ease(int cur, int target, int dt_ms, int tau_ms, int snap)
{
    int d = target - cur, k;

    if (dt_ms <= 0 || d == 0)
        return cur;
    if (dt_ms > 1000)
        dt_ms = 1000;
    k = dt_ms * 256 / (tau_ms + dt_ms); /* 1 - exp(-dt/tau), within a few percent */
    cur += d * k / 256;
    if (cur == target - d) /* the step rounded to nothing: one unit at least */
        cur += d > 0 ? 1 : -1;
    d = target - cur;
    return d < snap && d > -snap ? target : cur;
}

/* A 0..256 ramp that reaches 256 after ms milliseconds since start (an opening fade). */
static inline int pw_ramp(unsigned int now_ms, unsigned int start_ms, int ms)
{
    unsigned int t = now_ms - start_ms;

    return ms <= 0 || t >= (unsigned int)ms ? 256 : (int)(t * 256 / (unsigned int)ms);
}

/* Frame statistics of a screen, logged when it closes (how fast the menu really is), in us. */
typedef struct {
    unsigned int loops, frames;              /* vblanks waited, frames drawn */
    unsigned int t_first, t_last;            /* ms */
    unsigned int backdrop, content, present; /* sums over the drawn frames */
    unsigned int poll, worst;                /* waiting for the vblank (sum); slowest frame */
} pw_frame_stats;

#endif

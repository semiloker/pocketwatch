// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/volume.h"

#include "ui/anim.h"

void pw_volume_reset(pw_volume *v)
{
    v->known = 0;
    v->active = 0;
    v->alpha = 0;
    v->flash = 0;
}

int pw_volume_update(pw_volume *v, int level, int muted, int pressed, unsigned int now_ms)
{
    unsigned int since;
    int dt;

    level = level < 0 ? 0 : level > PW_VOLUME_MAX ? PW_VOLUME_MAX : level;
    muted = muted != 0;
    if (!v->known) { /* the menu just opened: nothing changed yet */
        v->known = 1;
        v->active = 0;
        v->level = level;
        v->muted = muted;
        v->shown = level * 256;
        v->alpha = v->flash = 0;
        v->up = 1;
        v->t_last = now_ms;
        return 0;
    }
    dt = (int)(now_ms - v->t_last);
    v->t_last = now_ms;
    if (level != v->level || muted != v->muted || pressed) {
        if (level != v->level)
            v->up = level > v->level;
        v->level = level;
        v->muted = muted;
        v->t_change = now_ms;
        v->active = 1;
    }
    if (!v->active)
        return 0;

    since = now_ms - v->t_change;
    v->shown = pw_ease(v->shown, v->level * 256, dt, PW_ANIM_TAU_MS, 4);
    v->flash = since >= PW_VOLUME_FLASH_MS ? 0 : 256 - (int)(since * 256 / PW_VOLUME_FLASH_MS);
    if (since < PW_VOLUME_HOLD_MS) {
        v->alpha += dt > 0 ? dt * 256 / PW_VOLUME_APPEAR_MS : 0;
        if (v->alpha > 256)
            v->alpha = 256;
    } else if (since < PW_VOLUME_HOLD_MS + PW_VOLUME_FADE_MS) {
        int a = 256 - (int)((since - PW_VOLUME_HOLD_MS) * 256 / PW_VOLUME_FADE_MS);

        if (a < v->alpha)
            v->alpha = a;
    } else { /* gone: one more frame without it */
        v->active = 0;
        v->alpha = 0;
        v->shown = v->level * 256;
    }
    return 1;
}

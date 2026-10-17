// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_UI_VOLUME_H
#define PW_UI_VOLUME_H

/*
 * The volume bar over our screens (UI-REQ-016): the system's own bar does not show over our
 * screen, so a change of the volume or of the mute, or a volume button pressed at either end,
 * shows ours. The level glides to the new value by time; the bar fades out a moment after the last
 * change. Portable: the menu feeds it the system's values every frame.
 */

#define PW_VOLUME_MAX 30 /* sceImposeGetParam(PSP_IMPOSE_MAIN_VOLUME) runs 0..30 */

#define PW_VOLUME_APPEAR_MS 100
#define PW_VOLUME_HOLD_MS   1500 /* on the screen after the last change */
#define PW_VOLUME_FADE_MS   300
#define PW_VOLUME_FLASH_MS  250 /* the step that just changed glows this long */

typedef struct {
    int known;             /* a first reading was taken (it shows nothing) */
    int active;            /* on the screen or fading */
    int level, muted;      /* the system's values now */
    int shown;             /* the drawn level in 1/256 steps, gliding to level */
    int alpha;             /* 0-256 */
    int flash;             /* 0-256: just changed */
    int up;                /* the last change went up */
    unsigned int t_change; /* ms of the last change or press */
    unsigned int t_last;   /* ms of the last update */
} pw_volume;

void pw_volume_reset(pw_volume *v); /* the next reading is the first again */

/* The system's level (0..PW_VOLUME_MAX) and mute at now_ms; pressed: a volume button went down
   since the last call. Returns 1 when a frame is needed: while the bar shows, and once more when
   it has gone. */
int pw_volume_update(pw_volume *v, int level, int muted, int pressed, unsigned int now_ms);

#endif

// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_INPUT_H
#define PW_INPUT_H

/* Button logic for the menu, independent of how the buttons are read (host-tested). */

#define PW_TAP_MAX_US 400000u /* a longer press is the system's mute, not our tap */

/* A short, clean tap of one button (the NOTE button opens the menu). */
typedef struct {
    int down;
    int spoiled; /* another button was involved, or held too long */
    unsigned int t_down;
} pw_tap;

/* Feed the button's state, whether any other button is down, and the time in us.
   Returns 1 once, on the release that ends a clean tap. */
int pw_tap_update(pw_tap *t, int button_down, int others_down, unsigned int now_us);

/* Edge detection with auto-repeat for the buttons in repeat_mask (the D-pad). */
typedef struct {
    unsigned int prev;
    unsigned int t_press;  /* when the repeating button went down */
    unsigned int t_repeat; /* last repeat */
} pw_keys;

#define PW_REPEAT_DELAY_US    400000u
#define PW_REPEAT_INTERVAL_US 100000u

/* Returns the buttons that went down now, plus repeats of held buttons in repeat_mask. */
unsigned int pw_keys_update(pw_keys *k, unsigned int buttons, unsigned int repeat_mask,
                            unsigned int now_us);

#endif

// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/input.h"

int pw_tap_update(pw_tap *t, int button_down, int others_down, unsigned int now_us)
{
    if (button_down) {
        if (!t->down) {
            t->down = 1;
            t->spoiled = others_down;
            t->t_down = now_us;
        } else if (others_down || now_us - t->t_down > PW_TAP_MAX_US) {
            t->spoiled = 1;
        }
        return 0;
    }
    if (!t->down)
        return 0;
    t->down = 0;
    return !t->spoiled && now_us - t->t_down <= PW_TAP_MAX_US;
}

unsigned int pw_keys_update(pw_keys *k, unsigned int buttons, unsigned int repeat_mask,
                            unsigned int now_us)
{
    unsigned int pressed = buttons & ~k->prev;
    unsigned int held = buttons & k->prev & repeat_mask;

    if (pressed & repeat_mask) {
        k->t_press = now_us;
        k->t_repeat = now_us;
    } else if (held && now_us - k->t_press >= PW_REPEAT_DELAY_US &&
               now_us - k->t_repeat >= PW_REPEAT_INTERVAL_US) {
        k->t_repeat = now_us;
        pressed |= held;
    }
    k->prev = buttons;
    return pressed;
}

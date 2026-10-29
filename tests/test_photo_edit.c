// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdlib.h>
#include <string.h>

#include "photo_edit.h"
#include "pwtest.h"
#include "ui/draw.h"

PW_TEST(photo_edit_turns_and_mirrors)
{
    /* 3 x 2: 0 1 2 / 3 4 5 */
    unsigned int px[6] = {0, 1, 2, 3, 4, 5}, *t;
    int w, h;

    t = hm_edit_turn(px, 3, 2, 1, 0, &w, &h); /* a quarter turn clockwise: 3 0 / 4 1 / 5 2 */
    CHECK(t && w == 2 && h == 3);
    CHECK(t[0] == 3 && t[1] == 0 && t[2] == 4 && t[3] == 1 && t[4] == 5 && t[5] == 2);
    free(t);
    t = hm_edit_turn(px, 3, 2, 2, 0, &w, &h); /* half: 5 4 3 / 2 1 0 */
    CHECK(t && w == 3 && h == 2 && t[0] == 5 && t[5] == 0);
    free(t);
    t = hm_edit_turn(px, 3, 2, 3, 0, &w, &h); /* three quarters: 2 5 / 1 4 / 0 3 */
    CHECK(t && t[0] == 2 && t[1] == 5 && t[4] == 0 && t[5] == 3);
    free(t);
    t = hm_edit_turn(px, 3, 2, 0, 1, &w, &h); /* the mirror: 2 1 0 / 5 4 3 */
    CHECK(t && t[0] == 2 && t[2] == 0 && t[3] == 5);
    free(t);
}

PW_TEST(photo_edit_steps_stay_in_range)
{
    hm_edit e;

    memset(&e, 0, sizeof(e));
    CHECK(!hm_edit_changed(&e));
    for (int i = 0; i < 9; i++)
        hm_edit_step(&e, HM_ED_LIGHT, 1);
    CHECK_EQ(e.light, HM_ED_RANGE);
    CHECK_EQ(hm_edit_step(&e, HM_ED_LIGHT, 1), 0);
    CHECK_EQ(hm_edit_step(&e, HM_ED_VIGNETTE, -1), 0); /* 0..5 */
    hm_edit_step(&e, HM_ED_ROTATE, -1);
    CHECK_EQ(e.rotate, 3);
    hm_edit_step(&e, HM_ED_FILTER, -1);
    CHECK_EQ(e.filter, HM_FILTERS - 1);
    CHECK(hm_edit_changed(&e));
}

PW_TEST(photo_edit_colours)
{
    static unsigned int px[64 * 32];
    hm_edit e;

    for (int i = 0; i < 64 * 32; i++)
        px[i] = PW_RGB(200, 60, 40);
    memset(&e, 0, sizeof(e));
    hm_edit_colours(&e, px, 64, 32, 64); /* nothing asked: as it was */
    CHECK(px[5] == PW_RGB(200, 60, 40));
    e.filter = HM_FL_MONO;
    hm_edit_colours(&e, px, 64, 32, 64);
    CHECK((px[5] & 0xFF) == (px[5] >> 8 & 0xFF) && (px[5] & 0xFF) == (px[5] >> 16 & 0xFF));
    for (int i = 0; i < 64 * 32; i++)
        px[i] = PW_RGB(128, 128, 128);
    memset(&e, 0, sizeof(e));
    e.vignette = HM_ED_RANGE; /* the corners darker than the middle */
    hm_edit_colours(&e, px, 64, 32, 64);
    CHECK((px[0] & 0xFF) < (px[16 * 64 + 32] & 0xFF));
    memset(&e, 0, sizeof(e));
    e.light = 3; /* brighter */
    for (int i = 0; i < 4; i++)
        px[i] = PW_RGB(100, 100, 100);
    hm_edit_colours(&e, px, 4, 1, 4);
    CHECK((px[0] & 0xFF) > 100);
}

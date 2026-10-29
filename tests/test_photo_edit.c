// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdlib.h>
#include <string.h>

#include "photo_edit.h"
#include "pwtest.h"
#include "ui/draw.h"

#define R(p) ((int)((p) & 0xFF))
#define G(p) ((int)((p) >> 8 & 0xFF))
#define B(p) ((int)((p) >> 16 & 0xFF))

static void fill(unsigned int *px, int n, unsigned int c)
{
    for (int i = 0; i < n; i++)
        px[i] = c;
}

/* One pixel through an edit. */
static unsigned int one(const hm_edit *e, unsigned int c)
{
    unsigned int p = c;

    hm_edit_colours(e, &p, 1, 1, 1);
    return p;
}

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

PW_TEST(photo_edit_tools_step_reset_and_stay_in_range)
{
    hm_edit e;

    hm_edit_init(&e);
    CHECK(!hm_edit_changed(&e));
    CHECK_EQ(hm_edit_tools(HM_EDT_ADJUST), HM_ADJS);
    CHECK_EQ(hm_edit_tools(HM_EDT_FILTERS), HM_FILTERS);
    CHECK_EQ(hm_edit_tools(HM_EDT_CROP), HM_CROPS);
    for (int i = 0; i < 30; i++)
        hm_edit_step(&e, HM_EDT_ADJUST, HM_ADJ_EXPOSURE, 1);
    CHECK_EQ(e.adj[HM_ADJ_EXPOSURE], HM_ADJ_MAX);
    CHECK_EQ(hm_edit_step(&e, HM_EDT_ADJUST, HM_ADJ_EXPOSURE, 1), 0);
    CHECK_EQ(hm_edit_step(&e, HM_EDT_ADJUST, HM_ADJ_SHADOWS, -1), 1); /* two-sided */
    CHECK_EQ(e.adj[HM_ADJ_SHADOWS], -HM_ADJ_STEP);
    CHECK_EQ(hm_edit_step(&e, HM_EDT_ADJUST, HM_ADJ_VIGNETTE, -1), 0); /* 0..100 */
    CHECK_EQ(hm_edit_step(&e, HM_EDT_ADJUST, HM_ADJ_SHARPEN, -1), 0);
    CHECK(hm_edit_is_set(&e, HM_EDT_ADJUST, HM_ADJ_EXPOSURE));
    CHECK(!hm_edit_is_set(&e, HM_EDT_ADJUST, HM_ADJ_TINT));
    CHECK_EQ(hm_edit_reset(&e, HM_EDT_ADJUST, HM_ADJ_EXPOSURE), 1);
    CHECK_EQ(e.adj[HM_ADJ_EXPOSURE], 0);
    CHECK_EQ(hm_edit_reset(&e, HM_EDT_ADJUST, HM_ADJ_EXPOSURE), 0); /* (already) */
    /* Filters: a look chosen at full strength, up / down on it its strength */
    CHECK_EQ(hm_edit_step(&e, HM_EDT_FILTERS, HM_FL_SEPIA, 1), 1);
    CHECK(e.filter == HM_FL_SEPIA && e.strength == 100);
    CHECK_EQ(hm_edit_step(&e, HM_EDT_FILTERS, HM_FL_SEPIA, -1), 1);
    CHECK_EQ(e.strength, 100 - HM_STR_STEP);
    CHECK_EQ(hm_edit_pick_filter(&e, HM_FL_NOIR), 1);
    CHECK_EQ(e.strength, 100);
    CHECK_EQ(hm_edit_step(&e, HM_EDT_FILTERS, HM_FL_NOIR, 1), 0); /* (at 100) */
    CHECK_EQ(hm_edit_reset(&e, HM_EDT_FILTERS, HM_FL_NOIR), 1);
    CHECK_EQ(e.filter, HM_FL_NONE);
    /* Crop: turns and ratios go round */
    hm_edit_step(&e, HM_EDT_CROP, HM_CROP_ROTATE, -1);
    CHECK_EQ(e.rotate, 3);
    hm_edit_step(&e, HM_EDT_CROP, HM_CROP_ASPECT, -1);
    CHECK_EQ(e.aspect, HM_ARS - 1);
    CHECK(hm_edit_changed(&e));
    hm_edit_reset(&e, HM_EDT_CROP, HM_CROP_ROTATE);
    hm_edit_reset(&e, HM_EDT_CROP, HM_CROP_ASPECT);
    hm_edit_reset(&e, HM_EDT_ADJUST, HM_ADJ_SHADOWS);
    CHECK(!hm_edit_changed(&e));
}

PW_TEST(photo_edit_nothing_asked_leaves_the_picture_as_it_was)
{
    static unsigned int px[40 * 20];
    hm_edit e;

    for (int i = 0; i < 40 * 20; i++)
        px[i] = PW_RGB(i & 0xFF, (i * 7) & 0xFF, (i * 13) & 0xFF);
    hm_edit_init(&e);
    CHECK_EQ(hm_edit_colours(&e, px, 40, 20, 40), 0);
    for (int i = 0; i < 40 * 20; i++)
        CHECK(px[i] == PW_RGB(i & 0xFF, (i * 7) & 0xFF, (i * 13) & 0xFF));
    e.filter = HM_FL_NOIR; /* a look at strength 0: nothing either */
    e.strength = 0;
    CHECK(one(&e, PW_RGB(200, 60, 40)) == PW_RGB(200, 60, 40));
}

PW_TEST(photo_edit_light_tools_move_the_right_tones)
{
    hm_edit e;
    unsigned int p;

    hm_edit_init(&e);
    e.adj[HM_ADJ_EXPOSURE] = 50;
    CHECK(R(one(&e, PW_RGB(100, 100, 100))) > 130);
    e.adj[HM_ADJ_EXPOSURE] = -50;
    CHECK(R(one(&e, PW_RGB(100, 100, 100))) < 90);
    hm_edit_init(&e);
    e.adj[HM_ADJ_CONTRAST] = 60; /* further from the middle */
    CHECK(R(one(&e, PW_RGB(80, 80, 80))) < 80 && R(one(&e, PW_RGB(180, 180, 180))) > 180);
    hm_edit_init(&e);
    e.adj[HM_ADJ_HIGHLIGHTS] = -100; /* the bright tones down, the dark ones kept */
    CHECK(R(one(&e, PW_RGB(230, 230, 230))) < 215);
    CHECK_EQ(R(one(&e, PW_RGB(60, 60, 60))), 60);
    hm_edit_init(&e);
    e.adj[HM_ADJ_SHADOWS] = 100; /* the dark tones up, the bright ones kept */
    CHECK(R(one(&e, PW_RGB(30, 30, 30))) > 45);
    CHECK_EQ(R(one(&e, PW_RGB(220, 220, 220))), 220);
    hm_edit_init(&e);
    e.adj[HM_ADJ_WARMTH] = 100;
    p = one(&e, PW_RGB(128, 128, 128));
    CHECK(R(p) > 128 && B(p) < 128);
    hm_edit_init(&e);
    e.adj[HM_ADJ_TINT] = 100; /* towards magenta: less green */
    p = one(&e, PW_RGB(128, 128, 128));
    CHECK(G(p) < 128 && R(p) > 128 && B(p) > 128);
}

PW_TEST(photo_edit_colour_tools_and_looks)
{
    hm_edit e;
    unsigned int dull, vivid;

    hm_edit_init(&e);
    e.adj[HM_ADJ_SATURATION] = -100; /* grey */
    dull = one(&e, PW_RGB(200, 60, 40));
    CHECK(R(dull) == G(dull) && G(dull) == B(dull));
    hm_edit_init(&e);
    e.adj[HM_ADJ_VIBRANCE] = 100; /* the dull colour gains more than the vivid one */
    dull = one(&e, PW_RGB(140, 120, 110));
    vivid = one(&e, PW_RGB(250, 20, 10));
    CHECK(R(dull) - B(dull) - (140 - 110) > R(vivid) - B(vivid) - (250 - 10));
    CHECK(R(dull) - B(dull) > 140 - 110 + 10);
    hm_edit_init(&e);
    e.filter = HM_FL_MONO;
    dull = one(&e, PW_RGB(200, 60, 40));
    CHECK(R(dull) == G(dull) && G(dull) == B(dull));
    e.filter = HM_FL_SEPIA; /* a warm brown */
    dull = one(&e, PW_RGB(120, 120, 120));
    CHECK(R(dull) > G(dull) && G(dull) > B(dull));
    e.strength = 50; /* half the look: between */
    vivid = one(&e, PW_RGB(200, 60, 40));
    CHECK(R(vivid) > G(vivid) + 40);
}

PW_TEST(photo_edit_vignette_and_sharpening)
{
    static unsigned int px[64 * 32];
    hm_edit e;

    fill(px, 64 * 32, PW_RGB(128, 128, 128));
    hm_edit_init(&e);
    e.adj[HM_ADJ_VIGNETTE] = 100; /* the corners darker than the middle */
    hm_edit_colours(&e, px, 64, 32, 64);
    CHECK(R(px[0]) < R(px[16 * 64 + 32]));
    /* an edge: dark half, bright half - sharpening pushes the two sides apart */
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 64; x++)
            px[y * 64 + x] = x < 32 ? PW_RGB(100, 100, 100) : PW_RGB(150, 150, 150);
    hm_edit_init(&e);
    e.adj[HM_ADJ_SHARPEN] = 100;
    CHECK_EQ(hm_edit_colours(&e, px, 64, 32, 64), 0);
    CHECK(R(px[10 * 64 + 31]) < 100 && R(px[10 * 64 + 32]) > 150);
    CHECK_EQ(R(px[10 * 64 + 10]), 100); /* flat: unchanged */
    CHECK_EQ(R(px[0 * 64 + 31]), 100);  /* the border rows are left */
}

PW_TEST(photo_edit_crops_centred_in_the_pictures_orientation)
{
    int x, y, w, h;

    hm_edit_crop(400, 300, HM_AR_ORIGINAL, &x, &y, &w, &h);
    CHECK(x == 0 && y == 0 && w == 400 && h == 300);
    hm_edit_crop(400, 300, HM_AR_SQUARE, &x, &y, &w, &h);
    CHECK(x == 50 && y == 0 && w == 300 && h == 300);
    hm_edit_crop(400, 300, HM_AR_16_9, &x, &y, &w, &h);
    CHECK(x == 0 && w == 400 && h == 225 && y == 37);
    hm_edit_crop(300, 400, HM_AR_16_9, &x, &y, &w, &h); /* portrait: 9:16 */
    CHECK(y == 0 && h == 400 && w == 225 && x == 37);
    hm_edit_crop(300, 400, HM_AR_4_3, &x, &y, &w, &h); /* already 3:4 */
    CHECK(x == 0 && y == 0 && w == 300 && h == 400);
    {
        unsigned int px[4 * 3] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

        hm_edit_crop_in_place(px, 4, 1, 1, 2, 2); /* rows 1-2, columns 1-2 */
        CHECK(px[0] == 5 && px[1] == 6 && px[2] == 9 && px[3] == 10);
    }
}

PW_TEST(photo_edit_histogram_counts_brightness)
{
    static unsigned int px[16 * 16];
    unsigned char h[HM_HIST_BINS];

    fill(px, 16 * 16, PW_RGB(0, 0, 0));
    fill(px, 64, PW_RGB(255, 255, 255)); /* a quarter white */
    hm_edit_histogram(px, 16, 16, 16, h);
    CHECK_EQ(h[0], 255);
    CHECK_EQ(h[HM_HIST_BINS - 1], 85);
    CHECK_EQ(h[HM_HIST_BINS / 2], 0);
}

PW_TEST(photo_edit_screen_fits_the_picture_into_the_canvas_and_shows_the_looks)
{
    static unsigned int bg[512 * HM_ED_SH], src[160 * 120], thumb[HM_ED_THUMBW * HM_ED_THUMBH];
    unsigned char hist[HM_HIST_BINS];
    unsigned int *frame;
    hm_edit e;
    int fw, fh, mid = (HM_ED_CY + HM_ED_CH / 2) * 512 + HM_ED_SW / 2;

    fill(src, 160 * 120, PW_RGB(200, 60, 40));
    fill(thumb, HM_ED_THUMBW * HM_ED_THUMBH, PW_RGB(200, 60, 40));
    hm_edit_init(&e);
    frame = hm_edit_frame(&e, src, 160, 120, &fw, &fh);
    CHECK(frame && fw <= HM_ED_CW && fh <= HM_ED_CH && fh == HM_ED_CH - 8);
    CHECK_EQ(hm_edit_compose(&e, frame, fw, fh, thumb, HM_ED_THUMBW, HM_ED_THUMBH, HM_EDT_ADJUST, 0,
                             bg, hist),
             0);
    CHECK(bg[mid] == PW_RGB(200, 60, 40));                     /* the picture in the canvas */
    CHECK(bg[(HM_ED_CY + 2) * 512 + 4] == HM_ED_DARK);         /* beside it: the background */
    CHECK(bg[HM_ED_STRIP * 512 + HM_ED_SW / 2] == HM_ED_DARK); /* Adjust: no thumbnails */
    CHECK_EQ(hist[99 >> 2], 255);                              /* (its brightness: 99 of 255) */
    e.filter = HM_FL_MONO; /* the same frame: only the colours change (#10) */
    hm_edit_compose(&e, frame, fw, fh, thumb, HM_ED_THUMBW, HM_ED_THUMBH, HM_EDT_FILTERS, 0, bg,
                    hist);
    CHECK(R(bg[mid]) == G(bg[mid])); /* the look applied */
    {
        int x0 = hm_edit_strip_x(HM_FILTERS, HM_ED_TPITCH, HM_FL_NONE),
            x1 = hm_edit_strip_x(HM_FILTERS, HM_ED_TPITCH, HM_FL_MONO);
        unsigned int a = bg[HM_ED_STRIP * 512 + x0], b = bg[HM_ED_STRIP * 512 + x1];

        CHECK(a == PW_RGB(200, 60, 40));     /* Original's thumbnail as it is */
        CHECK(R(b) == G(b) && G(b) == B(b)); /* Mono's grey */
    }
    hm_edit_compose(&e, frame, fw, fh, thumb, HM_ED_THUMBW, HM_ED_THUMBH, HM_EDT_FILTERS, 1, bg,
                    hist); /* compare */
    CHECK(bg[mid] == PW_RGB(200, 60, 40));
    free(frame);
    e.rotate = 1; /* a portrait now: narrower in the canvas */
    frame = hm_edit_frame(&e, src, 160, 120, &fw, &fh);
    CHECK(frame && fw < fh);
    hm_edit_compose(&e, frame, fw, fh, thumb, HM_ED_THUMBW, HM_ED_THUMBH, HM_EDT_CROP, 0, bg, hist);
    CHECK(bg[mid - 100] == HM_ED_DARK);
    free(frame);
    CHECK_EQ(
        hm_edit_compose(&e, 0, 0, 0, thumb, HM_ED_THUMBW, HM_ED_THUMBH, HM_EDT_CROP, 0, bg, hist),
        -1); /* no frame: dark */
}

PW_TEST(photo_edit_one_press_is_a_step_one_sees)
{
    /* a press of each Adjust tool moves a mid tone by a visible amount (#10: +5 of the old scales
       moved some by a level or two) */
    static const struct {
        int adj, from, at_least;
    } k[] = {{HM_ADJ_EXPOSURE, 128, 8}, {HM_ADJ_CONTRAST, 200, 4}, {HM_ADJ_HIGHLIGHTS, 220, 5},
             {HM_ADJ_SHADOWS, 40, 5},   {HM_ADJ_WARMTH, 128, 5},   {HM_ADJ_TINT, 128, 5}};
    hm_edit e;

    for (unsigned int i = 0; i < sizeof(k) / sizeof(k[0]); i++) {
        unsigned int p, q;
        int d;

        hm_edit_init(&e);
        q = PW_RGB(k[i].from, k[i].from, k[i].from);
        hm_edit_step(&e, HM_EDT_ADJUST, k[i].adj, 1);
        p = one(&e, q);
        d = R(p) - R(q) > G(q) - G(p) ? R(p) - R(q) : G(q) - G(p);
        if (d < 0)
            d = -d;
        CHECK(d >= k[i].at_least);
    }
    hm_edit_init(&e); /* exposure's whites stay white */
    e.adj[HM_ADJ_EXPOSURE] = 100;
    CHECK_EQ(R(one(&e, PW_RGB(255, 255, 255))), 255);
    CHECK(R(one(&e, PW_RGB(128, 128, 128))) > 190);
}

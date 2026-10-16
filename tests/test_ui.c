// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "pwtest.h"
#include "ui/draw.h"
#include "ui/input.h"

#define W 32
#define H 16

static unsigned int g_px[H * (W + 8)];

static pw_canvas canvas(void)
{
    pw_canvas c = {g_px, W, H, W + 8};

    memset(g_px, 0, sizeof(g_px));
    return c;
}

PW_TEST(draw_fill_is_clipped)
{
    pw_canvas c = canvas();

    pw_draw_fill(&c, -4, -4, 8, 8, 0xFFFFFFFFu);
    CHECK_EQ(g_px[0], 0xFFFFFFFFu);
    CHECK_EQ(g_px[3 * c.stride + 3], 0xFFFFFFFFu);
    CHECK_EQ(g_px[4 * c.stride + 4], 0);
    pw_draw_fill(&c, W - 2, H - 2, 10, 10, 0x11111111u);
    CHECK_EQ(g_px[(H - 1) * c.stride + W - 1], 0x11111111u);
    CHECK_EQ(g_px[(H - 1) * c.stride + W], 0); /* stride padding untouched */
}

PW_TEST(draw_blend_mixes_channels)
{
    pw_canvas c = canvas();

    pw_draw_fill(&c, 0, 0, 1, 1, PW_RGB(0, 100, 200));
    pw_draw_blend(&c, 0, 0, 1, 1, PW_RGB(200, 100, 0), 128);
    CHECK_EQ(g_px[0], PW_RGB(100, 100, 100));
}

PW_TEST(draw_text_renders_glyphs)
{
    pw_canvas c = canvas();

    /* '!' row 0 is 0x18: pixels 3 and 4 set. */
    CHECK_EQ(pw_draw_text(&c, 0, 0, "!", 0xFFFFFFFFu, 1), 8);
    CHECK_EQ(g_px[2], 0);
    CHECK_EQ(g_px[3], 0xFFFFFFFFu);
    CHECK_EQ(g_px[4], 0xFFFFFFFFu);
    CHECK_EQ(g_px[5], 0);
    CHECK_EQ(pw_draw_text_width("abc", 2), 48);
    pw_draw_text(&c, 0, 0, "\x01", 0xFFFFFFFFu, 1); /* unprintable = space: nothing new */
    CHECK_EQ(g_px[3], 0xFFFFFFFFu);
}

PW_TEST(draw_from_frame_converts_formats)
{
    pw_canvas c = canvas();
    unsigned short f16[W * H];
    unsigned int f32[W * H];

    for (int i = 0; i < W * H; i++)
        f16[i] = 0xF800; /* 565: pure blue */
    pw_draw_from_frame(&c, f16, W, PW_FMT_565, 256);
    CHECK_EQ(g_px[0], PW_RGB(0, 0, 255));

    for (int i = 0; i < W * H; i++)
        f16[i] = 0x001F; /* 5551: pure red */
    pw_draw_from_frame(&c, f16, W, PW_FMT_5551, 256);
    CHECK_EQ(g_px[c.stride + 1], PW_RGB(255, 0, 0));

    for (int i = 0; i < W * H; i++)
        f16[i] = 0x00F0; /* 4444: pure green */
    pw_draw_from_frame(&c, f16, W, PW_FMT_4444, 256);
    CHECK_EQ(g_px[5], PW_RGB(0, 255, 0));

    for (int i = 0; i < W * H; i++)
        f32[i] = PW_RGB(200, 100, 50);
    pw_draw_from_frame(&c, f32, W, PW_FMT_8888, 128); /* half brightness */
    CHECK_EQ(g_px[0], PW_RGB(100, 50, 25));
}

PW_TEST(tap_short_clean_press_counts)
{
    pw_tap t = {0, 0, 0};

    CHECK_EQ(pw_tap_update(&t, 1, 0, 1000), 0);
    CHECK_EQ(pw_tap_update(&t, 1, 0, 200000), 0);
    CHECK_EQ(pw_tap_update(&t, 0, 0, 300000), 1);
    CHECK_EQ(pw_tap_update(&t, 0, 0, 400000), 0); /* only once */
}

PW_TEST(tap_long_hold_or_combo_does_not_count)
{
    pw_tap t = {0, 0, 0};

    pw_tap_update(&t, 1, 0, 0);
    pw_tap_update(&t, 1, 0, PW_TAP_MAX_US + 1); /* held: the system's mute */
    CHECK_EQ(pw_tap_update(&t, 0, 0, PW_TAP_MAX_US + 2), 0);

    pw_tap_update(&t, 1, 0, 1000000);
    pw_tap_update(&t, 1, 1, 1001000); /* another button joined */
    CHECK_EQ(pw_tap_update(&t, 0, 0, 1002000), 0);

    pw_tap_update(&t, 1, 1, 2000000); /* another button was already down */
    CHECK_EQ(pw_tap_update(&t, 0, 0, 2001000), 0);
}

PW_TEST(keys_edges_and_repeat)
{
    pw_keys k = {0, 0, 0};
    unsigned int dpad = 0xF0, down = 0x40, cross = 0x4000;

    CHECK_EQ(pw_keys_update(&k, down, dpad, 0), down);
    CHECK_EQ(pw_keys_update(&k, down, dpad, 100000), 0);
    CHECK_EQ(pw_keys_update(&k, down, dpad, PW_REPEAT_DELAY_US), down);
    CHECK_EQ(pw_keys_update(&k, down, dpad, PW_REPEAT_DELAY_US + 50000), 0);
    CHECK_EQ(pw_keys_update(&k, down, dpad, PW_REPEAT_DELAY_US + PW_REPEAT_INTERVAL_US), down);
    CHECK_EQ(pw_keys_update(&k, down | cross, dpad, 520000), cross); /* 20 ms after a repeat */
    CHECK_EQ(pw_keys_update(&k, down | cross, dpad, 2000000), down); /* cross does not repeat */
    CHECK_EQ(pw_keys_update(&k, 0, dpad, 2100000), 0);
}

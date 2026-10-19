// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdlib.h>
#include <string.h>

#include "gfx_cpu.h"
#include "home_view.h"
#include "pwtest.h"
#include "xmbart.h"

PW_TEST(home_rows_skip_group_names_and_remarks)
{
    hm_row r[5] = {
        {"a", 0, -1, HM_ROW_HEADER, 0, 0}, {"b", 0, -1, HM_ROW_INFO, 0, 0},
        {"c", 0, -1, HM_ROW_NOTE, 0, 0},   {"d", 0, -1, HM_ROW_ACTION, 0, 0},
        {"e", 0, -1, HM_ROW_HEADER, 0, 0},
    };

    CHECK_EQ(hm_row_next(r, 5, -1, 1), 1); /* the first that can be chosen */
    CHECK_EQ(hm_row_next(r, 5, 1, 1), 3);  /* over the remark */
    CHECK_EQ(hm_row_next(r, 5, 3, 1), 3);  /* only a group's name after it: stays */
    CHECK_EQ(hm_row_next(r, 5, 3, -1), 1);
    CHECK_EQ(hm_row_next(r, 5, 1, -1), 1);
    CHECK_EQ(hm_row_next(r, 0, -1, 1), -1);
}

static unsigned int g_px[512 * HM_H];

/* Every section of every style with nothing in it, and with everything over it at once: drawn
   without reading past anything (the sanitizers watch), and something drawn. */
PW_TEST(home_draws_every_style_empty_and_full)
{
    pw_canvas c = {g_px, HM_W, HM_H, 512};
    pw_font title, body;
    hm_ui ui = {&title, &body, &title, 1, 0};
    hm_gfx_cpu g;
    hm_row one = {"No music", 0, -1, HM_ROW_INFO, 0, 0};

    pw_font_init_fallback(&title, 2);
    pw_font_init_fallback(&body, 1);
    hm_gfx_cpu_init(&g, &c);
    for (int style = 0; style < HM_STYLES; style++)
        for (int tab = 0; tab < HM_TABS; tab++) {
            hm_view v;

            memset(&v, 0, sizeof(v));
            memset(g_px, 0, sizeof(g_px));
            v.style = style;
            v.tab = tab;
            v.tab_pos = tab * 256;
            v.appear = 256;
            v.battery = -1;
            v.mu_cur = -1;
            v.mu_bar = -1;
            v.wave = 256;
            if (tab == HM_TAB_MUSIC) { /* no tracks: the one row that says so */
                v.rows = &one;
                v.nrows = 1;
            }
            hm_draw(&g.ops, &ui, &v);
            v.panel = v.sheet = v.popup = 256; /* all at once, none filled in */
            hm_draw(&g.ops, &ui, &v);
            hm_prefetch(&g.ops, &ui, &v);
            CHECK(g_px[0] != 0 || g_px[HM_W / 2 + 512 * (HM_H / 2)] != 0);
        }
}

static unsigned char *no_file(const char *path, unsigned int *len)
{
    (void)path;
    *len = 0;
    return NULL;
}

PW_TEST(home_art_without_the_files_is_empty)
{
    hm_art a;
    pw_image sky;

    CHECK_EQ(hm_art_load(&a, no_file, "x.rco", "y.rco"), 0);
    for (int i = 0; i < HM_TABS; i++)
        CHECK(a.cat[i].px == NULL);
    CHECK(a.battery.px == NULL);
    CHECK(hm_art_sky(&sky, no_file, "z.bmp", 3) < 0);
    CHECK(sky.px == NULL);
}

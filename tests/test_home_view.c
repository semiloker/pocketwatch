// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdlib.h>
#include <string.h>

#include "gfx_cpu.h"
#include "hcfg.h"
#include "home_view.h"
#include "pwtest.h"
#include "savedata.h"
#include "xmbart.h"

static hm_row mk(const char *label, int kind)
{
    hm_row r;

    memset(&r, 0, sizeof(r));
    r.label = label;
    r.kind = kind;
    r.bar = -1;
    return r;
}

PW_TEST(home_rows_skip_group_names_and_remarks)
{
    hm_row r[5] = {mk("a", HM_ROW_HEADER), mk("b", HM_ROW_INFO), mk("c", HM_ROW_NOTE),
                   mk("d", HM_ROW_ACTION), mk("e", HM_ROW_HEADER)};

    CHECK_EQ(hm_row_next(r, 5, -1, 1), 1); /* the first that can be chosen */
    CHECK_EQ(hm_row_next(r, 5, 1, 1), 3);  /* over the remark */
    CHECK_EQ(hm_row_next(r, 5, 3, 1), 3);  /* only a group's name after it: stays */
    CHECK_EQ(hm_row_next(r, 5, 3, -1), 1);
    CHECK_EQ(hm_row_next(r, 5, 1, -1), 1);
    CHECK_EQ(hm_row_next(r, 0, -1, 1), -1);
}

static unsigned int g_px[512 * HM_H];

/* Every layout of every style and theme, empty and with everything over it at once, and in the
   middle of a page sliding in: drawn without reading past anything (the sanitizers watch), and
   something drawn. */
PW_TEST(home_draws_every_page_in_every_style)
{
    static const int layouts[] = {HM_PAGE_ROWS,  HM_PAGE_GAMES, HM_PAGE_ALBUMS, HM_PAGE_PHOTOS,
                                  HM_PAGE_SAVES, HM_PAGE_MUSIC, HM_PAGE_NOW};
    pw_canvas c = {g_px, HM_W, HM_H, 512};
    pw_font title, body;
    hm_ui ui = {&title, &body, &title, 1, 0, 0, 0, {0, 0, 0}};
    hm_gfx_cpu g;
    hm_row one = mk("No music", HM_ROW_INFO);
    hm_tile tile = {"A game", "ULUS10277", 0, 1};

    pw_font_init_fallback(&title, 2);
    pw_font_init_fallback(&body, 1);
    hm_gfx_cpu_init(&g, &c);
    for (int theme = 0; theme < HM_THEMES; theme++)
        for (int style = 0; style < HM_STYLES; style++)
            for (unsigned int k = 0; k < sizeof(layouts) / sizeof(layouts[0]); k++)
                for (int full = 0; full < 2; full++) {
                    static hm_view v;

                    memset(&v, 0, sizeof(v));
                    memset(g_px, 0, sizeof(g_px));
                    ui.theme = theme;
                    ui.accent = hm_cfg_accent(theme);
                    v.style = style;
                    v.tab = (int)k % HM_TABS;
                    v.tab_pos = v.tab * 256;
                    v.appear = 256;
                    v.battery = 50;
                    v.battery_pct = 1;
                    v.mu_cur = -1;
                    v.mu_bar = full ? 100 : -1;
                    v.mu_title = full ? "Track" : 0;
                    v.wave = 256;
                    v.aurora = full ? 256 : 0;
                    v.page[0].layout = HM_PAGE_ROWS;
                    v.page[0].rows = &one;
                    v.page[0].nrows = 1;
                    v.page[1].layout = layouts[k];
                    v.page[1].title = full ? "A page" : 0;
                    v.page[1].icon = 0;
                    v.page[1].over = full && layouts[k] == HM_PAGE_ROWS;
                    v.page[1].rows = &one;
                    v.page[1].nrows = layouts[k] == HM_PAGE_ROWS || layouts[k] == HM_PAGE_MUSIC;
                    v.page[1].tiles = &tile;
                    v.page[1].count = full;
                    v.depth = 1;
                    v.depth_pos = full ? 128 : 256; /* sliding in, and there */
                    hm_draw(&g.ops, &ui, &v);
                    if (full) {
                        v.panel = v.sheet = v.popup = v.toast = 256; /* all at once, empty */
                        v.toast_text = "Saved";
                        hm_draw(&g.ops, &ui, &v);
                        v.viewer = v.editor = 256;
                        hm_draw(&g.ops, &ui, &v);
                    }
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
    for (int i = 0; i < HM_ART_ITEMS; i++)
        CHECK(a.item[i].px == NULL);
    CHECK(a.battery.px == NULL);
    CHECK(hm_art_sky(&sky, no_file, "z.bmp", 3) < 0);
    CHECK(sky.px == NULL);
}

PW_TEST(home_settings_file_round_trip)
{
    hm_cfg c, d;
    char buf[1024];
    int n;

    hm_cfg_defaults(&c);
    CHECK_EQ(c.wave, 1);
    CHECK_EQ(c.percent, 1);
    CHECK_EQ(c.repeat, HM_REPEAT_ALL);
    CHECK_EQ(c.hold, HM_HOLD_MUSIC);
    c.accent = 7;
    c.theme = HM_THEME_PAPER;
    c.bg = HM_BG_PLAIN;
    c.sky = 11;
    c.wave = 0;
    c.shuffle = 1;
    c.repeat = HM_REPEAT_ONE;
    c.motion = HM_MOTION_CALM;
    c.hold = HM_HOLD_ALWAYS;
    c.clock = HM_CLOCK_DAY;
    c.perf = 1;
    n = hm_cfg_format(&c, buf, sizeof(buf));
    CHECK(n > 0);
    hm_cfg_defaults(&d);
    CHECK_EQ(d.perf, 0);
    CHECK_EQ(hm_cfg_parse(&d, buf, (unsigned int)n), 12);
    CHECK(memcmp(&c, &d, sizeof(c)) == 0);
    /* unknown keys and bad values: ignored, the rest taken */
    hm_cfg_defaults(&d);
    {
        static const char t[] = "accent = puce\nsky = 13\nsky=system\n# theme = oled\n"
                                "theme   =  oled  \r\nnonsense\nwave = maybe\n";

        CHECK_EQ(hm_cfg_parse(&d, t, sizeof(t) - 1), 2);
    }
    CHECK_EQ(d.accent, 0);
    CHECK_EQ(d.sky, 0);
    CHECK_EQ(d.theme, HM_THEME_OLED);
    CHECK_EQ(d.wave, 1);
    CHECK(hm_cfg_tau(HM_MOTION_FAST, 50) < 50);
    CHECK(hm_cfg_tau(HM_MOTION_CALM, 50) > 50);
    CHECK(hm_cfg_accent(99) == hm_cfg_accent(0));
}

PW_TEST(home_save_detail_first_line)
{
    char out[16];

    hm_saves_first_line("\nChapter 3\nThe castle", 22, out, sizeof(out));
    CHECK(strcmp(out, "Chapter 3") == 0);
    hm_saves_first_line("A very long line that is cut", 28, out, sizeof(out));
    CHECK(strcmp(out, "A very long lin") == 0);
    hm_saves_first_line("", 0, out, sizeof(out));
    CHECK(out[0] == 0);
}

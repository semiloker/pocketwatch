// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Renders every menu screen on the host into PNGs, for reviewing the look without a PSP:
 *
 *   make -C tests preview FONT=ltn0.pgf RCO=topmenu_icon.rco SKY=01-12_03g.bmp MONTH=10
 *
 * Every input is optional (built-in font, drawn icons, a synthetic sky). Output: PNGs in
 * tests/build/preview. Never commit renders made with Sony files
 * (docs/assets/references/README.md).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/png.h"
#include "gfx/image.h"
#include "gfx/rco.h"
#include "gfx/rlz.h"
#include "gfx/text.h"
#include "ui/backdrop.h"
#include "ui/i18n.h"
#include "ui/icons.h"
#include "ui/shapes.h"
#include "ui/views.h"
#include "ui/xmbbg.h"
#include "gfx_cpu.h"
#include "hm_icons.h"
#include "home_view.h"
#include "xmbart.h"

#define W      480
#define H      272
#define STRIDE 512

static unsigned int g_px[STRIDE * H], g_sky_page[STRIDE * H], g_game[STRIDE * H];
static unsigned char g_pool[3][72 * 1024], g_scratch[3][64 * 1024];
static pw_pgf g_pgf;
static pw_font g_title, g_body, g_big;

enum { A_CAT, A_UMD, A_UMDG, A_FOLD, A_FOLDG, A_UPD, A_UPDG, A_MS, A_MSG, A_SET, A_COUNT };
static const char *k_labels[A_COUNT] = {"AO", "BH", "BP", "BJ", "BR", "EH", "EP", "BI", "BQ", "BX"};
static pw_image g_art[A_COUNT];
static int g_have[A_COUNT];
static int g_style;
static const char *const k_prefix[PW_STYLE_COUNT] = {"", "modern-", "minimal-"};

static unsigned char *slurp(const char *path, unsigned int *len)
{
    FILE *f = fopen(path, "rb");
    unsigned char *buf;
    long n;

    if (f == NULL)
        return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc((size_t)n + 1);
    *len = (unsigned int)fread(buf, 1, (size_t)n, f);
    fclose(f);
    return buf;
}

static void row_rgb(void *ctx, unsigned int y, unsigned char *rgb)
{
    const unsigned int *row = (const unsigned int *)ctx + y * STRIDE;

    for (int x = 0; x < W; x++) {
        rgb[3 * x + 0] = (unsigned char)(row[x] & 0xFF);
        rgb[3 * x + 1] = (unsigned char)((row[x] >> 8) & 0xFF);
        rgb[3 * x + 2] = (unsigned char)((row[x] >> 16) & 0xFF);
    }
}

static void save(const char *name)
{
    static unsigned char png[512 * 1024], scratch[1 + 3 * W];
    char path[256];
    int n = pw_png_encode_rgb(W, H, row_rgb, g_px, scratch, png, sizeof(png));
    FILE *f;

    snprintf(path, sizeof(path), "build/preview/%s%s.png", k_prefix[g_style], name);
    f = fopen(path, "wb");
    if (f == NULL || n < 0) {
        fprintf(stderr, "cannot write %s\n", path);
        exit(1);
    }
    fwrite(png, 1, (size_t)n, f);
    fclose(f);
    printf("%s\n", path);
}

static void load_icons(const char *path)
{
    static unsigned char tables[64 * 1024], gim[64 * 1024], state[PW_RLZ_STATE];
    unsigned int len = 0;
    unsigned char *file = path && path[0] ? slurp(path, &len) : NULL;
    pw_rco r;

    if (file == NULL || pw_rco_open(&r, file, len, tables, sizeof(tables), state, 0) < 0)
        return;
    for (int i = 0; i < A_COUNT; i++) {
        pw_rco_image im;
        int got;

        if (pw_rco_find_image(&r, k_labels[i], &im) < 0 ||
            (got = pw_rco_read_image(&r, &im, gim, sizeof(gim), state, 0)) < 0)
            continue;
        unsigned int *px = malloc(512 * 512 * 4);

        if (pw_gim_decode(gim, (unsigned int)got, px, 512 * 512, &g_art[i]) == 0)
            g_have[i] = 1;
    }
}

static const pw_image *art(int i)
{
    return g_have[i] ? &g_art[i] : NULL;
}

/* A stand-in game frame: a dusk sky, hills, a castle, a hero and a health bar. */
static void synthetic_game(void)
{
    pw_canvas g = {g_game, W, H, STRIDE};

    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            g_game[y * STRIDE + x] = PW_RGB(40 + y / 3, 30 + y / 4, 90 + y / 5);
    pw_shape_disc(&g, PW_FX(380), PW_FX(70), PW_FX(34), PW_RGB(250, 210, 140), 256);
    pw_shape_disc(&g, PW_FX(90), PW_FX(330), PW_FX(170), PW_RGB(40, 90, 60), 256);
    pw_shape_disc(&g, PW_FX(360), PW_FX(360), PW_FX(190), PW_RGB(30, 70, 50), 256);
    pw_draw_fill(&g, 250, 110, 70, 90, PW_RGB(70, 60, 80));
    pw_draw_fill(&g, 240, 90, 20, 40, PW_RGB(80, 70, 90));
    pw_draw_fill(&g, 310, 90, 20, 40, PW_RGB(80, 70, 90));
    pw_draw_fill(&g, 276, 160, 18, 40, PW_RGB(30, 20, 30));
    pw_shape_disc(&g, PW_FX(150), PW_FX(196), PW_FX(9), PW_RGB(240, 200, 170), 256);
    pw_draw_fill(&g, 142, 205, 16, 26, PW_RGB(170, 30, 40));
    pw_draw_fill(&g, 16, 14, 120, 8, PW_RGB(30, 30, 30));
    pw_draw_fill(&g, 18, 16, 84, 4, PW_RGB(220, 40, 50));
}

/* A stand-in sky when no BMP is given: a blue gradient. */
static void synthetic_sky(pw_image *sky)
{
    static unsigned int px[60 * 34];

    for (int y = 0; y < 34; y++)
        for (int x = 0; x < 60; x++)
            px[y * 60 + x] =
                PW_RGB(18 + y * 40 / 34 + x / 6, 34 + y * 70 / 34 + x / 4, 92 + y * 90 / 34);
    sky->px = px;
    sky->w = 60;
    sky->h = 34;
}

static pw_canvas begin(unsigned int t)
{
    pw_canvas c = {g_px, W, H, STRIDE};

    memcpy(g_px, g_sky_page, sizeof(g_px));
    pw_backdrop_frame(&c, t, g_style);
    return c;
}

/* PW_PREVIEW_LANG=ru renders with plugin/lang/ru.txt (the font may lack its letters). */
static void load_language(const char *code)
{
    char path[128];
    unsigned int n = 0;
    unsigned char *d;

    if (code == NULL || code[0] == 0)
        return;
    snprintf(path, sizeof(path), "../plugin/lang/%s.txt", code);
    d = slurp(path, &n);
    if (d == NULL || pw_i18n_load((char *)d, n, NULL, NULL) < 0)
        fprintf(stderr, "%s: no usable translation\n", path);
}

/* Pocketwatch Home (home/): every page in the three styles (and Modern in each theme), with
   synthetic games. The system's icons when PW_PREVIEW_RCO / PW_PREVIEW_FG name
   topmenu_icon.rco / system_plugin_fg.rco. */
static hm_row hrow(const char *label, const char *value, int kind)
{
    hm_row r;

    memset(&r, 0, sizeof(r));
    r.label = label;
    r.value = value;
    r.kind = kind;
    r.bar = -1;
    return r;
}

static hm_ui g_hui;
static hm_gfx_cpu g_hg;

static void hshot(hm_view *v, const char *name)
{
    hm_draw(&g_hg.ops, &g_hui, v);
    save(name);
}

/* One page over the section's first (depth 1), settled. */
static void over(hm_view *v, const hm_page *root, const hm_page *p)
{
    v->page[0] = *root;
    v->page[1] = *p;
    v->depth = 1;
    v->depth_pos = 256;
}

static void home_previews(const pw_image *sky_img)
{
    static unsigned int icons[4][HM_ICON_W * HM_ICON_H], pic[HM_W * HM_H], bg[512 * HM_H],
        pic1[512 * HM_H], grad[512 * HM_H], sky[512 * HM_H], photo[512 * HM_H],
        cover[HM_COVER * HM_COVER], coverbg[512 * HM_H], plain[512 * HM_H];
    static const unsigned int tint[4] = {PW_RGB(170, 40, 40), PW_RGB(40, 110, 190),
                                         PW_RGB(60, 150, 80), PW_RGB(150, 110, 40)};
    static hm_art art;
    static const int ed_glyph[9] = {HM_ICON_ROTATE,  HM_ICON_FLIP,     HM_ICON_FILTER,
                                    HM_ICON_SUN,     HM_ICON_CONTRAST, HM_ICON_DROP,
                                    HM_ICON_PALETTE, HM_ICON_VIGNETTE, HM_ICON_CHECK};
    static const char *const ed_names[9] = {"Rotate",     "Mirror",   "Filter",
                                            "Brightness", "Contrast", "Saturation",
                                            "Warmth",     "Vignette", "Save a Copy"};
    const char *rco = getenv("PW_PREVIEW_RCO"), *fg = getenv("PW_PREVIEW_FG");
    hm_tile games[5] = {
        {"Castlevania: The Dracula X Chronicles", "ULUS10277   Played 12 h 30 min   Save states: 3",
         icons[0], 1},
        {"Persona", "ULUS10432   Played 4 h 12 min", icons[1], 0},
        {"The 3rd Birthday", "ULES01513   Save states: 1", icons[2], 0},
        {"Silent Hill: Shattered Memories", "ULUS10450", icons[3], 0},
        {"Untitled image", NULL, NULL, 0},
    };
    hm_tile photos[9],
        albums[7] = {{"All Photos", "23 photos", icons[1], 0},
                     {"Castlevania: The Dracula X Chronicles", "9 photos", icons[0], 0},
                     {"Screenshots", "11 photos", icons[2], 0},
                     {"pspIdent", "3 photos", 0, 0},
                     {"Need for Speed", "1 photos", icons[3], 0},
                     {"PICTURE", "8 photos", icons[2], 0},
                     {"ULUS10509", "1 photos", icons[0], 0}};
    hm_tile saves[4] = {{"Chapter 3: The Clock Tower", "2026/10/18 14:05   1.2 MB", icons[0], 0},
                        {"Richter - Stage 2", "2026/10/12 21:40   1.2 MB", icons[2], 0},
                        {"System Data", "2026/09/30 08:12   48 KB", 0, 0},
                        {"Bonus", "2026/09/01 10:00   12 KB", icons[3], 0}};
    hm_row playtime[5], sysr[7], setr[7], look[8], game[4], battery[5], save1[6];
    hm_row music[7];
    hm_page p, root;
    hm_view v;
    pw_canvas c = {g_px, W, H, STRIDE};

    if (rco && rco[0])
        printf("home: %d system picture(s)\n", hm_art_load(&art, slurp, rco, fg ? fg : ""));
    for (int i = 0; i < 4; i++) /* stand-in icons: a tinted gradient with a ring */
        for (int y = 0; y < HM_ICON_H; y++)
            for (int x = 0; x < HM_ICON_W; x++) {
                unsigned int t = tint[i];
                int k = 120 + x * 136 / HM_ICON_W, d = (x - 72) * (x - 72) + (y - 40) * (y - 40);

                icons[i][y * HM_ICON_W + x] =
                    d > 500 && d < 700 ? PW_RGB(255, 255, 255)
                                       : PW_RGB((t & 0xFF) * k / 256, ((t >> 8) & 0xFF) * k / 256,
                                                ((t >> 16) & 0xFF) * k / 256);
            }
    for (int y = 0; y < HM_COVER; y++) /* a stand-in cover: a sunset and a disc */
        for (int x = 0; x < HM_COVER; x++) {
            int d = (x - 64) * (x - 64) + (y - 76) * (y - 76);

            cover[y * HM_COVER + x] =
                d < 900 ? PW_RGB(250, 196, 90) : PW_RGB(200 - y / 2, 60 + y / 3, 120 + x / 4);
        }
    for (int i = 0; i < 9; i++)
        photos[i] = (hm_tile){i == 0 ? "Sunset at the pier.jpg" : "Photo", "PICTURE   2026/10/18",
                              icons[i % 4], 0};
    for (int y = 0; y < HM_H; y++) /* the synthetic game frame as the first game's PIC1 */
        memcpy(pic + y * HM_W, g_game + y * STRIDE, HM_W * 4);
    hm_make_background(pic, HM_W, HM_H, bg);
    hm_make_pic1(pic, HM_W, HM_H, pic1);
    hm_make_gradient(PW_RGB(0, 114, 206), grad);
    hm_make_sky(sky_img, sky);
    hm_make_photo(pic, HM_W, HM_H, photo);
    hm_make_background(cover, HM_COVER, HM_COVER, coverbg);
    hm_make_plain(PW_RGB(0, 114, 206), 0, plain);

    playtime[0] = hrow("Total", "17 h 17 min", HM_ROW_INFO);
    playtime[1] = hrow("Castlevania: The Dracula X Chronicles", "12 h 30 min", HM_ROW_INFO);
    playtime[1].bar = 256;
    playtime[1].icon = icons[0];
    playtime[2] = hrow("Persona", "4 h 12 min", HM_ROW_INFO);
    playtime[2].bar = 86;
    playtime[2].icon = icons[1];
    playtime[3] = hrow("The 3rd Birthday", "35 min", HM_ROW_INFO);
    playtime[3].bar = 12;
    playtime[3].icon = icons[2];
    playtime[4] = hrow("Counted only while Pocketwatch runs", NULL, HM_ROW_NOTE);
    {
        static const char *const names[7] = {"USB Connection", "Internet Browser", "Battery",
                                             "Memory Stick",   "About This PSP",   "Power",
                                             "Exit to the XMB"};
        static const char *const vals[7] = {0, 0, "83%", "2310 MB free", "PSP-3000", 0, 0};
        static const int kinds[7] = {HM_ROW_ACTION, HM_ROW_ACTION, HM_ROW_LINK,  HM_ROW_LINK,
                                     HM_ROW_LINK,   HM_ROW_LINK,   HM_ROW_ACTION};
        static const int glyphs[7] = {HM_ICON_USB,    HM_ICON_GLOBE, HM_ICON_BATTERY, PW_ICON_CARD,
                                      HM_ICON_SYSTEM, PW_ICON_QUIT,  HM_ICON_EXIT};
        static const int arts[7] = {HM_ART_USB + 1,    HM_ART_WWW + 1,   0, HM_ART_MS + 1,
                                    HM_ART_SYSTEM + 1, HM_ART_POWER + 1, 0};

        for (int i = 0; i < 7; i++) {
            sysr[i] = hrow(names[i], vals[i], kinds[i]);
            sysr[i].glyph = glyphs[i];
            sysr[i].art = arts[i];
        }
    }
    {
        static const char *const names[7] = {
            "Appearance",    "Pocketwatch", "Music and Photos", "Sound and Display",
            "Date and Time", "System",      "Network"};
        static const char *const vals[7] = {"Modern", 0, 0, 0, 0, "English", 0};
        static const int glyphs[7] = {HM_ICON_PALETTE, HM_ICON_GAMES, HM_ICON_MUSIC,
                                      HM_ICON_SPEAKER, HM_ICON_CLOCK, HM_ICON_LANGUAGE,
                                      HM_ICON_GLOBE};
        static const int arts[7] = {HM_ART_THEME + 1,  HM_ART_UMD + 1,   HM_ART_PHOTO + 1,
                                    HM_ART_SOUND + 1,  HM_ART_CLOCK + 1, HM_ART_SYSTEM + 1,
                                    HM_ART_NETWORK + 1};

        for (int i = 0; i < 7; i++) {
            setr[i] = hrow(names[i], vals[i], HM_ROW_LINK);
            setr[i].glyph = glyphs[i];
            setr[i].art = arts[i];
        }
    }
    look[0] = hrow("Style", "Modern", HM_ROW_CHOICE);
    look[1] = hrow("Theme", "Glass", HM_ROW_CHOICE);
    look[2] = hrow("Accent Colour", "Violet", HM_ROW_CHOICE);
    look[2].swatch = PW_RGB(128, 84, 230);
    look[3] = hrow("Background", "XMB Sky", HM_ROW_CHOICE);
    look[4] = hrow("Sky Colour", "October", HM_ROW_CHOICE);
    look[4].swatch = PW_RGB(180, 140, 60);
    look[5] = hrow("Wave", "On", HM_ROW_CHOICE);
    look[5].toggle = 2;
    look[6] = hrow("Battery Percentage", "On", HM_ROW_CHOICE);
    look[6].toggle = 2;
    look[7] = hrow("Animations", "Normal", HM_ROW_CHOICE);
    game[0] = hrow("Play", 0, HM_ROW_ACTION);
    game[0].glyph = HM_ICON_PLAY;
    game[1] = hrow("Save States", "3", HM_ROW_LINK);
    game[1].glyph = PW_ICON_SAVE;
    game[2] = hrow("Saved Data", 0, HM_ROW_LINK);
    game[2].glyph = HM_ICON_FOLDER;
    game[3] = hrow("Information", 0, HM_ROW_LINK);
    game[3].glyph = HM_ICON_SYSTEM;
    battery[0] = hrow("Charge", "83%   On battery", HM_ROW_INFO);
    battery[0].bar = 212;
    battery[1] = hrow("Time left", "5 h 37 min (system)", HM_ROW_INFO);
    battery[2] = hrow("Voltage", "4.10 V", HM_ROW_INFO);
    battery[3] = hrow("Temperature", "28 C", HM_ROW_INFO);
    battery[4] = hrow("Charge cycles", "112", HM_ROW_INFO);
    save1[0] = hrow("Game", "Castlevania: The Dracula X Chronicles", HM_ROW_INFO);
    save1[1] = hrow("Details", "Chapter 3: The Clock Tower", HM_ROW_INFO);
    save1[2] = hrow("Saved", "2026/10/18 14:05", HM_ROW_INFO);
    save1[3] = hrow("Size", "1.2 MB", HM_ROW_INFO);
    save1[4] = hrow("Folder", "ULUS10277000", HM_ROW_INFO);
    save1[5] = hrow("Delete", 0, HM_ROW_ACTION);
    save1[5].glyph = HM_ICON_TRASH;
    music[0] = hrow("Midnight Drive", "1:24", HM_ROW_INFO);
    music[0].bar = 94;
    {
        static const char *const t[6][2] = {
            {"Neon Rain", "Synthwave Trio"}, {"Midnight Drive", "Lumen"},
            {"Paper Planes", "Kite Club"},   {"Low Tide", "Harbour"},
            {"Static Hearts", "Lumen"},      {"Glass City", "Neon Rain"}};

        for (int i = 0; i < 6; i++)
            music[i + 1] = hrow(t[i][0], t[i][1], HM_ROW_ACTION);
        music[2].bar = 94;
    }

    g_hui =
        (hm_ui){&g_title,      &g_body,  &g_big, 1, rco && rco[0] ? &art : 0, PW_RGB(0, 114, 206),
                HM_THEME_DARK, {0, 0, 0}};
    hm_gfx_cpu_init(&g_hg, &c);
    for (g_style = 0; g_style < HM_STYLES; g_style++) {
        int cl = g_style == HM_STYLE_CLASSIC;

        memset(&v, 0, sizeof(v));
        v.style = g_style;
        v.tab_names[HM_TAB_GAMES] = "Games";
        v.tab_names[HM_TAB_MUSIC] = "Music";
        v.tab_names[HM_TAB_PHOTOS] = "Photos";
        v.tab_names[HM_TAB_PLAYTIME] = "Play Time";
        v.tab_names[HM_TAB_SETTINGS] = "Settings";
        v.tab_names[HM_TAB_SYSTEM] = "System";
        v.clock = "10/18 14:05";
        v.battery = 83;
        v.battery_pct = 1;
        v.bg = cl ? pic1 : bg;
        v.bg_mix = 256;
        v.play_label = "Play";
        v.hint_enter = "Play";
        v.hint_options = "Options";
        v.hint_tabs = cl ? 0 : "Section";
        v.appear = 256;
        v.mu_cur = -1;
        v.t_ms = 1200;

        /* Games */
        memset(&root, 0, sizeof(root));
        root.layout = HM_PAGE_GAMES;
        root.tiles = games;
        root.count = 5;
        root.empty = "No games in ms0:/ISO";
        root.is_dash = g_style == HM_STYLE_MODERN; /* Modern's landing dashboard (ADR-050) */
        v.page[0] = root;
        hshot(&v, "home-games");
        v.page[0].sel = 1;
        v.page[0].selpos = 128 + 64; /* gliding to the second game */
        v.bg = cl ? sky : grad;
        v.wave = cl ? 256 : 0;
        hshot(&v, "home-games-moving");
        v.page[0] = root;
        v.page[0].count = 0;
        hshot(&v, "home-empty");
        v.bg = cl ? pic1 : bg;
        v.wave = 0;
        /* Triangle: the game's page */
        memset(&p, 0, sizeof(p));
        p.layout = HM_PAGE_ROWS;
        p.over = 1;
        p.title = games[0].title;
        p.sub = games[0].meta;
        p.icon = icons[0];
        p.rows = game;
        p.nrows = 4;
        p.row_sel = 1;
        p.row_pos = 256;
        over(&v, &root, &p);
        v.hint_enter = "Open";
        v.hint_options = 0;
        v.hint_back = "Back";
        hshot(&v, "home-game-page");
        v.depth_pos = 128; /* sliding in */
        hshot(&v, "home-game-page-sliding");
        /* its saved data, one save */
        memset(&p, 0, sizeof(p));
        p.layout = HM_PAGE_SAVES;
        p.title = "Saved Data";
        p.sub = games[0].title;
        p.tiles = saves;
        p.count = 4;
        p.sel = 0;
        over(&v, &root, &p);
        v.hint_square = "Delete";
        hshot(&v, "home-saved-data");
        memset(&p, 0, sizeof(p));
        p.layout = HM_PAGE_ROWS;
        p.title = saves[0].title;
        p.sub = games[0].title;
        p.icon = icons[0];
        p.rows = save1;
        p.nrows = 6;
        p.row_sel = 5;
        p.row_pos = 5 * 256;
        over(&v, &root, &p);
        v.hint_square = 0;
        v.hint_enter = "Delete";
        hshot(&v, "home-save");
        v.sheet = 256; /* Delete: the question */
        v.sheet_title = "Delete";
        v.sheet_text = "Delete this saved data? This cannot be undone.";
        v.sheet_hint = "Cancel";
        v.sheet_again = "Delete";
        v.sheet_glyph = HM_ICON_TRASH;
        hshot(&v, "home-delete");
        v.sheet = 0;
        v.depth = v.depth_pos = 0;
        v.hint_back = 0;

        /* Music */
        memset(&root, 0, sizeof(root));
        root.layout = HM_PAGE_MUSIC;
        root.rows = music;
        root.nrows = 7;
        root.row_sel = 2;
        root.row_pos = 2 * 256;
        v.page[0] = root;
        v.tab = HM_TAB_MUSIC;
        v.tab_pos = HM_TAB_MUSIC * 256;
        v.mu_title = "Midnight Drive";
        v.mu_artist = "Lumen";
        v.mu_album = "Night Roads";
        v.mu_time = "1:24";
        v.mu_length = "3:51";
        v.mu_bar = 94;
        v.mu_state = 1;
        v.mu_cur = 1;
        v.mu_cover = cover;
        v.mu_repeat = 1;
        v.bg = cl ? sky : coverbg;
        v.wave = cl ? 256 : 0;
        v.hint_enter = "Play";
        v.hint_options = "Now Playing";
        v.hint_square = "Stop";
        hshot(&v, "home-music");
        memset(&p, 0, sizeof(p));
        p.layout = HM_PAGE_NOW;
        p.title = "Now Playing";
        over(&v, &root, &p);
        v.hint_enter = "Pause";
        v.hint_square = "Shuffle";
        v.hint_options = "Repeat";
        v.hint_back = "Back";
        v.hint_lr = "Seek";
        v.hint_ud = "Track";
        v.mu_shuffle = 1;
        hshot(&v, "home-now-playing");
        v.mu_cover = 0; /* without a cover */
        v.mu_seek = 1;
        hshot(&v, "home-now-no-cover");
        v.mu_seek = 0;
        v.hint_lr = v.hint_ud = 0;
        { /* Home's keyboard (the nickname), its Cyrillic page, the question on Back */
            hm_kb kb;

            hm_kb_open(&kb, "Nickname", "Semiloker", 32, 0);
            kb.row = 2;
            kb.col = 4;
            v.kb = &kb;
            v.kb_t = 256;
            hshot(&v, "home-keyboard");
            kb.page = HM_KB_CYRILLIC;
            kb.shift = 1;
            hshot(&v, "home-keyboard-cyrillic");
            kb.ask = 1;
            hshot(&v, "home-keyboard-ask");
            v.kb = 0;
            v.kb_t = 0;
        }
        { /* the battery's calibration (measuring, its chart), the volume and brightness pill */
            static unsigned char pct[90];
            hm_calview c;
            hm_page cal;

            memset(&c, 0, sizeof(c));
            for (int i = 0; i < 90; i++)
                pct[i] = (unsigned char)(100 - i * 62 / 89);
            c.step = 3;
            c.steps[0] = "Charge";
            c.steps[1] = "Unplug";
            c.steps[2] = "Run down";
            c.steps[3] = "Result";
            c.title = "Battery Calibration";
            c.what = "Measuring. Leave Home on until the PSP turns itself off, then charge it and "
                     "open this page again.";
            c.big = "38%";
            c.big_sub = "On battery";
            c.label[0] = "Elapsed";
            c.value[0] = "3 h 05 min";
            c.label[1] = "Used";
            c.value[1] = "1104 mAh";
            c.label[2] = "Average";
            c.value[2] = "358 mA";
            c.label[3] = "Full charge";
            c.value[3] = "~1780 mAh";
            c.nstats = 4;
            c.pct = pct;
            c.npct = 90;
            c.chart_left = "100%";
            c.chart_right = "3 h 05 min";
            memset(&cal, 0, sizeof(cal));
            cal.layout = HM_PAGE_CAL;
            cal.title = c.title;
            over(&v, &v.page[0], &cal);
            v.cal = &c;
            v.hint_enter = "Stop";
            hshot(&v, "home-calibration");
            v.cal = 0;
            v.depth = v.depth_pos = 0;
            v.osd = 256;
            v.osd_glyph = HM_ICON_SUN;
            v.osd_level = 92 * 256 / 100;
            v.osd_text = "4/4   92%";
            hshot(&v, "home-osd");
            v.osd = 0;
        }
        v.depth = v.depth_pos = 0;
        v.hint_back = v.hint_square = v.hint_options = 0;

        /* Photos: the albums, one album, a photo, the editor */
        memset(&root, 0, sizeof(root));
        root.layout = HM_PAGE_ALBUMS;
        root.tiles = albums;
        root.count = 7;
        root.sel = 1;
        root.selpos = 256;
        root.grid_pos = 0; /* (Modern: the first row) */
        v.page[0] = root;
        v.tab = HM_TAB_PHOTOS;
        v.tab_pos = HM_TAB_PHOTOS * 256;
        v.bg = cl ? sky : bg;
        v.hint_enter = "Open";
        hshot(&v, "home-albums");
        memset(&p, 0, sizeof(p));
        p.layout = HM_PAGE_PHOTOS;
        p.title = albums[1].title;
        p.tiles = photos;
        p.count = 9;
        p.sel = 5;
        p.selpos = 5 * 256;
        p.grid_pos = 5 / HM_PHOTO_COLS * 256;
        over(&v, &root, &p);
        v.hint_enter = "View";
        v.hint_square = "Delete";
        v.hint_back = "Back";
        hshot(&v, "home-album");
        v.bg = photo;
        v.viewer = 256;
        v.caption = 256;
        v.viewer_caption = "Sunset at the pier.jpg";
        v.viewer_edit = "Edit";
        v.viewer_slideshow = "Slideshow";
        v.viewer_wallpaper = "Set as wallpaper";
        hshot(&v, "home-viewer");
        v.editor = 256;
        v.ed_count = 9;
        for (int i = 0; i < 9; i++)
            v.ed_tools[i] = ed_names[i];
        v.ed_glyphs = ed_glyph;
        v.ed_sel = v.ed_pos = 3;
        v.ed_pos = 3 * 256;
        v.ed_value = "+2";
        v.ed_hint = "Left / Right: tool   Up / Down: change";
        hshot(&v, "home-editor");
        v.editor = v.viewer = 0;
        v.toast = 256;
        v.toast_text = "Saved as a new photo";
        v.bg = cl ? sky : bg;
        hshot(&v, "home-album-saved");
        v.toast = 0;
        v.depth = v.depth_pos = 0;
        v.hint_back = v.hint_square = 0;

        /* Play Time */
        memset(&root, 0, sizeof(root));
        root.layout = HM_PAGE_ROWS;
        root.rows = playtime;
        root.nrows = 5;
        root.row_sel = 1;
        root.row_pos = 256;
        v.page[0] = root;
        v.tab = HM_TAB_PLAYTIME;
        v.tab_pos = HM_TAB_PLAYTIME * 256;
        v.bg = cl ? sky : grad;
        v.hint_enter = "Open";
        hshot(&v, "home-playtime");

        /* Settings: the categories, Appearance, a choice's options */
        memset(&root, 0, sizeof(root));
        root.layout = HM_PAGE_ROWS;
        root.rows = setr;
        root.nrows = 7;
        v.page[0] = root;
        v.tab = HM_TAB_SETTINGS;
        v.tab_pos = HM_TAB_SETTINGS * 256;
        hshot(&v, "home-settings");
        memset(&p, 0, sizeof(p));
        p.layout = HM_PAGE_ROWS;
        p.title = "Appearance";
        p.rows = look;
        p.nrows = 8;
        p.row_sel = 2;
        p.row_pos = 2 * 256;
        over(&v, &root, &p);
        v.hint_enter = cl ? "Change" : 0;
        v.hint_back = "Back";
        hshot(&v, "home-settings-look");
        v.depth_pos = 140;
        hshot(&v, "home-settings-sliding");
        v.depth_pos = 256;
        if (cl) { /* Enter on a choice: its options on the right */
            static const char *const accents[5] = {"Blue", "Violet", "Pink", "Red", "Orange"};
            static const unsigned int sw[5] = {PW_RGB(0, 114, 206), PW_RGB(128, 84, 230),
                                               PW_RGB(232, 72, 140), PW_RGB(226, 58, 58),
                                               PW_RGB(240, 130, 30)};

            v.popup = 256;
            v.popup_count = 5;
            for (int i = 0; i < 5; i++) {
                v.popup_items[i] = accents[i];
                v.popup_swatch[i] = sw[i];
            }
            v.popup_sel = 1;
            v.popup_pos = 256;
            v.hint_enter = "OK";
            hshot(&v, "home-settings-options");
            v.popup = 0;
        }
        v.depth = v.depth_pos = 0;
        v.hint_back = 0;

        /* System, Battery */
        memset(&root, 0, sizeof(root));
        root.layout = HM_PAGE_ROWS;
        root.rows = sysr;
        root.nrows = 7;
        root.row_sel = 2;
        root.row_pos = 2 * 256;
        v.page[0] = root;
        v.tab = HM_TAB_SYSTEM;
        v.tab_pos = HM_TAB_SYSTEM * 256;
        v.hint_enter = "Open";
        hshot(&v, "home-system");
        memset(&p, 0, sizeof(p));
        p.layout = HM_PAGE_ROWS;
        p.title = "Battery";
        p.rows = battery;
        p.nrows = 5;
        over(&v, &root, &p);
        v.hint_enter = 0;
        v.hint_back = "Back";
        hshot(&v, "home-battery");
        v.depth = v.depth_pos = 0;
        v.hint_back = 0;

        /* HOME: the panel over the games; USB connecting */
        memset(&root, 0, sizeof(root));
        root.layout = HM_PAGE_GAMES;
        root.tiles = games;
        root.count = 5;
        v.page[0] = root;
        v.tab = HM_TAB_GAMES;
        v.tab_pos = 0;
        v.bg = cl ? pic1 : bg;
        v.wave = 0;
        v.panel = 256;
        v.panel_title = "Pocketwatch";
        v.panel_items[0] = "Close";
        v.panel_items[1] = "USB Connection";
        v.panel_items[2] = "Exit to the XMB";
        v.panel_items[3] = "Sleep";
        v.panel_items[4] = "Restart";
        v.panel_items[5] = "Turn Off";
        v.panel_glyph[0] = HM_ICON_HOME;
        v.panel_glyph[1] = HM_ICON_USB;
        v.panel_glyph[2] = HM_ICON_EXIT;
        v.panel_glyph[3] = HM_ICON_SLEEP;
        v.panel_glyph[4] = HM_ICON_ROTATE;
        v.panel_glyph[5] = PW_ICON_QUIT;
        v.panel_count = 6;
        v.panel_sel = 2;
        v.panel_pos = 2 * 256;
        hshot(&v, "home-panel");
        v.panel = 0;
        memset(v.panel_glyph, 0, sizeof(v.panel_glyph));
        v.sheet = 256;
        v.sheet_busy = 1;
        v.sheet_title = "USB Connection";
        v.sheet_text = "Connecting to the computer...";
        v.sheet_hint = "End";
        v.sheet_again = "Connect Again";
        v.sheet_glyph = HM_ICON_USB;
        v.t_ms = 300;
        hshot(&v, "home-usb");
        v.sheet = 0;
    }
    /* Modern in each theme and accent: the settings' Appearance and the game page */
    g_style = HM_STYLE_MODERN;
    for (int theme = 0; theme < HM_THEMES; theme++) {
        static const char *const names[HM_THEMES] = {"dark", "glass", "oled", "paper"};
        static const unsigned int acc[HM_THEMES] = {PW_RGB(0, 114, 206), PW_RGB(128, 84, 230),
                                                    PW_RGB(40, 168, 90), PW_RGB(232, 72, 140)};
        char name[64];

        g_hui.theme = theme;
        g_hui.accent = acc[theme];
        memset(&v, 0, sizeof(v));
        v.style = HM_STYLE_MODERN;
        v.tab_names[HM_TAB_GAMES] = "Games";
        v.tab_names[HM_TAB_MUSIC] = "Music";
        v.tab_names[HM_TAB_PHOTOS] = "Photos";
        v.tab_names[HM_TAB_PLAYTIME] = "Play Time";
        v.tab_names[HM_TAB_SETTINGS] = "Settings";
        v.tab_names[HM_TAB_SYSTEM] = "System";
        v.clock = "10/18 14:05";
        v.battery = 83;
        v.battery_pct = 1;
        v.bg = theme == HM_THEME_PAPER ? plain : theme == HM_THEME_GLASS ? bg : grad;
        v.bg_mix = 256;
        v.appear = 256;
        v.tab = HM_TAB_SETTINGS;
        v.tab_pos = HM_TAB_SETTINGS * 256;
        v.hint_back = "Back";
        memset(&root, 0, sizeof(root));
        root.layout = HM_PAGE_ROWS;
        root.rows = setr;
        root.nrows = 7;
        memset(&p, 0, sizeof(p));
        p.layout = HM_PAGE_ROWS;
        p.title = "Appearance";
        p.rows = look;
        p.nrows = 8;
        p.row_sel = 2;
        p.row_pos = 2 * 256;
        over(&v, &root, &p);
        snprintf(name, sizeof(name), "home-theme-%s", names[theme]);
        hshot(&v, name);
        v.page[0] = root;
        v.depth = v.depth_pos = 0;
        v.hint_back = 0;
        v.hint_enter = "Open";
        snprintf(name, sizeof(name), "home-theme-%s-settings", names[theme]);
        hshot(&v, name);
    }
    { /* the aurora: three lights in the accent's colours over the night (Modern, Classic) */
        static unsigned int glow[3][HM_GLOW * HM_GLOW], night[512 * HM_H];
        unsigned int c = PW_RGB(128, 84, 230);

        for (int k = 0; k < 3; k++) {
            hm_make_glow(c, glow[k]);
            g_hui.glow[k] = glow[k];
            c = (c & 0xFF) << 8 | (c >> 8 & 0xFF) << 16 | (c >> 16 & 0xFF);
        }
        hm_make_plain(PW_RGB(128, 84, 230), 1, night);
        g_hui.theme = HM_THEME_GLASS;
        g_hui.accent = PW_RGB(128, 84, 230);
        v.bg = night;
        v.aurora = 256;
        v.t_ms = 40000;
        v.page[0] = root;
        v.depth = v.depth_pos = 0;
        hshot(&v, "home-theme-aurora");
        g_style = HM_STYLE_CLASSIC;
        v.style = HM_STYLE_CLASSIC;
        hshot(&v, "home-theme-aurora");
        g_hui.glow[0] = g_hui.glow[1] = g_hui.glow[2] = 0;
    }
    g_hui.theme = HM_THEME_DARK;
    g_style = 0;
}

int main(void)
{
    const char *font = getenv("PW_PREVIEW_FONT"), *sky_path = getenv("PW_PREVIEW_SKY");
    const char *month = getenv("PW_PREVIEW_MONTH");
    pw_ui ui;
    pw_canvas c;
    pw_image sky;
    pw_hub_cat cat;

    pw_font_init_fallback(&g_title, 2);
    pw_font_init_fallback(&g_body, 1);
    pw_font_init_fallback(&g_big, 2);
    if (font && font[0]) {
        unsigned int n = 0;
        unsigned char *d = slurp(font, &n);

        if (d == NULL || pw_pgf_open(&g_pgf, d, n) < 0 ||
            pw_font_init(&g_title, &g_pgf, 205, g_pool[0], sizeof(g_pool[0]), g_scratch[0],
                         sizeof(g_scratch[0])) < 0 ||
            pw_font_init(&g_body, &g_pgf, 160, g_pool[1], sizeof(g_pool[1]), g_scratch[1],
                         sizeof(g_scratch[1])) < 0 ||
            pw_font_init(&g_big, &g_pgf, 256, g_pool[2], sizeof(g_pool[2]), g_scratch[2],
                         sizeof(g_scratch[2])) < 0) {
            fprintf(stderr, "%s: not a usable PGF\n", font);
            return 1;
        }
    }
    load_icons(getenv("PW_PREVIEW_RCO"));
    load_language(getenv("PW_PREVIEW_LANG"));
    synthetic_sky(&sky);
    if (sky_path && sky_path[0]) {
        static unsigned int px[64 * 64];
        unsigned int n = 0;
        unsigned char *d = slurp(sky_path, &n);
        int m = month && month[0] ? atoi(month) : 10;

        if (d == NULL || pw_bmp_decode(d, n, m - 1, px, 64 * 64, &sky) < 0)
            synthetic_sky(&sky);
    }
    synthetic_game();

    for (g_style = 0; g_style < PW_STYLE_COUNT; g_style++) {
        {
            pw_canvas s = {g_sky_page, W, H, STRIDE}, game = {g_game, W, H, STRIDE};

            pw_backdrop_make(&s, &game, &sky, g_style);
        }
        memset(&ui, 0, sizeof(ui));
        ui.style = g_style;
        ui.title = &g_title;
        ui.body = &g_body;
        ui.cross_enter = 1;
        cat.icon = art(A_CAT);
        cat.fallback = PW_ICON_WATCH;
        cat.label = pw_tr(PW_S_GAME);
        cat.subtitle = "Castlevania: The Dracula X Chronicles";

        char used[64], held[64];
        snprintf(used, sizeof(used), pw_tr(PW_S_SLOTS_USED), 2, 5);
        snprintf(held, sizeof(held), pw_tr(PW_S_SLOT_SAVED_AT), 2, "14:05");
        pw_hub_row rows[] = {
            {art(A_UMD), art(A_UMDG), PW_ICON_RESUME, pw_tr(PW_S_RESUME_GAME), NULL, 1},
            {art(A_FOLD), art(A_FOLDG), PW_ICON_SAVE, pw_tr(PW_S_SAVE_STATE), used, 1},
            {art(A_UPD), art(A_UPDG), PW_ICON_LOAD, pw_tr(PW_S_LOAD_STATE), held, 1},
            {NULL, NULL, PW_ICON_QUIT, pw_tr(PW_S_QUIT_GAME), NULL, 1},
            {art(A_MS), art(A_MSG), PW_ICON_CARD, "Memory Stick Test", "Debug: test T1", 1},
        };

        c = begin(0);
        pw_view_status(&c, &ui, "10/16 14:05", 80, 0, 256, 0);
        pw_view_hub(&c, &ui, &cat, rows, 5, 0, 256);
        save("hub-resume");
        c = begin(1500);
        pw_view_status(&c, &ui, "10/16 14:05", 80, 0, 256, 0);
        pw_view_hub(&c, &ui, &cat, rows, 5, 256 * 2, 256);
        save("hub-load");
        c = begin(3000);
        pw_view_status(&c, &ui, "10/16 14:05", 20, 1, 256, 0);
        pw_view_hub(&c, &ui, &cat, rows, 5, 256 + 128, 256);
        save("hub-moving");
        {
            /* the volume bar while a step fills in, and muted */
            pw_volume vol = {1, 1, 18, 0, 17 * 256 + 160, 256, 200, 1, 0, 0};

            c = begin(1500);
            pw_view_status(&c, &ui, "10/16 14:05", 80, 0, 256, 0);
            pw_view_hub(&c, &ui, &cat, rows, 5, 0, 256);
            pw_view_volume(&c, &ui, &vol);
            save("hub-volume");
            vol.muted = 1;
            vol.flash = 0;
            vol.shown = 18 * 256;
            c = begin(1500);
            pw_view_status(&c, &ui, "10/16 14:05", 80, 0, 256, 0);
            pw_view_hub(&c, &ui, &cat, rows, 5, 0, 256);
            pw_view_volume(&c, &ui, &vol);
            save("hub-volume-muted");
        }
        rows[2].enabled = 0;
        rows[2].desc = pw_tr(PW_S_NO_SAVED_STATE);
        c = begin(4500);
        pw_view_status(&c, &ui, "10/16 14:05", 80, 0, 256, 0);
        pw_view_hub(&c, &ui, &cat, rows, 5, 256 * 2, 256);
        save("hub-load-disabled");
        c = begin(6000);
        pw_view_status(&c, &ui, "10/16 14:05", 80, 0, 128, 0);
        pw_view_hub(&c, &ui, &cat, rows, 5, 0, 128);
        pw_draw_lerp(&c, g_game, g_px, 128);
        save("hub-fading-in");
        c = begin(7000);
        pw_view_dialog(&c, &ui, PW_VIEW_CONFIRM, pw_tr(PW_S_CONFIRM_QUIT), NULL, 0, 256);
        save("dialog-quit");
        c = begin(8000);
        pw_view_dialog(&c, &ui, PW_VIEW_CONFIRM, pw_tr(PW_S_CONFIRM_LOAD),
                       pw_tr(PW_S_CONFIRM_LOAD2), 0, 256);
        save("dialog-confirm");
        c = begin(9000);
        pw_view_dialog(&c, &ui, PW_VIEW_MESSAGE, pw_tr(PW_S_G_BATTERY), pw_tr(PW_S_H_BATTERY), 1,
                       256);
        save("dialog-message");
        c = begin(10000);
        {
            char pct[64];

            snprintf(pct, sizeof(pct), pw_tr(PW_S_SAVING_PCT), 42);
            pw_view_busy(&c, &ui, pct, 10000);
        }
        save("busy");

        static unsigned int thumb[144 * 80];
        for (int y = 0; y < 80; y++)
            for (int x = 0; x < 144; x++)
                thumb[y * 144 + x] = 0xFF000000u | g_game[(y * 272 / 80) * STRIDE + x * 480 / 144];
        char t[5][64], info[2][64];
        for (int i = 0; i < 5; i++)
            snprintf(t[i], sizeof(t[i]), pw_tr(i < 2 ? PW_S_SLOT_TITLE : PW_S_NEW_SLOT), i + 1);
        snprintf(info[0], sizeof(info[0]), pw_tr(PW_S_SIZE_MB), "2026/10/16 14:05", 18u, 2u);
        snprintf(info[1], sizeof(info[1]), pw_tr(PW_S_SIZE_MB), "2026/10/15 22:41", 17u, 9u);
        pw_slot_row slots[] = {
            {1, thumb, t[0], info[0]}, {1, thumb, t[1], info[1]}, {0, NULL, t[2], NULL},
            {0, NULL, t[3], NULL},     {0, NULL, t[4], NULL},
        };
        c = begin(11000);
        pw_view_slots(&c, &ui, pw_tr(PW_S_SAVE_STATE), "Castlevania: The Dracula X Chronicles",
                      slots, 5, 1 * 256, 1, 256);
        save("slots-save");
        c = begin(12000);
        pw_view_slots(&c, &ui, pw_tr(PW_S_SAVE_STATE), "Castlevania: The Dracula X Chronicles",
                      slots, 5, 2 * 256, 1, 256);
        save("slots-new");
        c = begin(12500); /* half way from slot 2 to slot 3, the list fading in */
        pw_view_slots(&c, &ui, pw_tr(PW_S_SAVE_STATE), "Castlevania: The Dracula X Chronicles",
                      slots, 5, 256 + 128, 1, 200);
        save("slots-moving");
    }
    home_previews(&sky);
    return 0;
}

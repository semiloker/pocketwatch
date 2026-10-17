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

#define W      480
#define H      272
#define STRIDE 512

static unsigned int g_px[STRIDE * H], g_sky_page[STRIDE * H], g_game[STRIDE * H];
static unsigned char g_pool[2][64 * 1024], g_scratch[2][32 * 1024];
static pw_pgf g_pgf;
static pw_font g_title, g_body;

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
    if (font && font[0]) {
        unsigned int n = 0;
        unsigned char *d = slurp(font, &n);

        if (d == NULL || pw_pgf_open(&g_pgf, d, n) < 0 ||
            pw_font_init(&g_title, &g_pgf, 205, g_pool[0], sizeof(g_pool[0]), g_scratch[0],
                         sizeof(g_scratch[0])) < 0 ||
            pw_font_init(&g_body, &g_pgf, 160, g_pool[1], sizeof(g_pool[1]), g_scratch[1],
                         sizeof(g_scratch[1])) < 0) {
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

        pw_hub_cat scat = {art(A_SET), PW_ICON_GEAR, pw_tr(PW_S_SETTINGS), NULL};
        static const int style_names[PW_STYLE_COUNT] = {PW_S_STYLE_XMB, PW_S_STYLE_MODERN,
                                                        PW_S_STYLE_MINIMAL};
        pw_setting_row set[] = {
            {pw_tr(PW_S_SET_STYLE), pw_tr(style_names[g_style]), -1},
            {pw_tr(PW_S_SET_FAST), pw_tr(PW_S_ON), 1},
            {pw_tr(PW_S_SET_STATUS), pw_tr(PW_S_OFF), 0},
            {pw_tr(PW_S_SET_LANGUAGE), pw_tr(PW_S_SYSTEM_LANGUAGE), -1},
        };
        const char *langs[] = {pw_tr(PW_S_SYSTEM_LANGUAGE),
                               "English",
                               "Français",
                               "Español",
                               "Deutsch",
                               "Italiano",
                               "Nederlands",
                               "Português",
                               "Русский",
                               "Українська"};
        c = begin(13000);
        pw_view_settings(&c, &ui, &scat, set, 4, 1, NULL, 0, -1, 256);
        save("settings");
        c = begin(14000);
        pw_view_settings(&c, &ui, &scat, set, 4, 3, langs, 10, 8, 256);
        save("settings-language");
    }
    return 0;
}

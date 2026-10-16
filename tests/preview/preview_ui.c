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
#include "ui/icons.h"
#include "ui/views.h"
#include "ui/xmbbg.h"

#define W      480
#define H      272
#define STRIDE 512

static unsigned int g_px[STRIDE * H], g_sky_page[STRIDE * H], g_game[STRIDE * H];
static unsigned char g_pool[2][64 * 1024], g_scratch[2][32 * 1024];
static pw_pgf g_pgf;
static pw_font g_title, g_body;

enum { A_CAT, A_UMD, A_UMDG, A_FOLD, A_FOLDG, A_UPD, A_UPDG, A_MS, A_MSG, A_COUNT };
static const char *k_labels[A_COUNT] = {"AO", "BH", "BP", "BJ", "BR", "EH", "EP", "BI", "BQ"};
static pw_image g_art[A_COUNT];
static int g_have[A_COUNT];

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

    snprintf(path, sizeof(path), "build/preview/%s.png", name);
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
    pw_xmbbg_wave(&c, t);
    return c;
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
    synthetic_sky(&sky);
    if (sky_path && sky_path[0]) {
        static unsigned int px[64 * 64];
        unsigned int n = 0;
        unsigned char *d = slurp(sky_path, &n);
        int m = month && month[0] ? atoi(month) : 10;

        if (d == NULL || pw_bmp_decode(d, n, m - 1, px, 64 * 64, &sky) < 0)
            synthetic_sky(&sky);
    }
    {
        pw_canvas s = {g_sky_page, W, H, STRIDE};

        pw_xmbbg_sky(&s, &sky);
    }
    for (int y = 0; y < H; y++) /* a "game" frame for the fade */
        for (int x = 0; x < W; x++)
            g_game[y * STRIDE + x] = PW_RGB(x / 3, 120, 255 - y);

    memset(&ui, 0, sizeof(ui));
    ui.title = &g_title;
    ui.body = &g_body;
    ui.cross_enter = 1;
    cat.icon = art(A_CAT);
    cat.fallback = PW_ICON_WATCH;
    cat.label = "Game";

    pw_hub_row rows[] = {
        {art(A_UMD), art(A_UMDG), PW_ICON_RESUME, "Resume Game", NULL, 1},
        {art(A_FOLD), art(A_FOLDG), PW_ICON_SAVE, "Save State", "Saved at 14:05 (4.2 s)", 1},
        {art(A_UPD), art(A_UPDG), PW_ICON_LOAD, "Load State", "Saved at 14:05", 1},
        {NULL, NULL, PW_ICON_QUIT, "Quit Game", NULL, 1},
        {art(A_MS), art(A_MSG), PW_ICON_CARD, "Memory Stick Test", "Debug: test T1", 1},
    };

    c = begin(0);
    pw_view_status(&c, &ui, "10/16 14:05", 80, 0, 256);
    pw_view_hub(&c, &ui, &cat, rows, 5, 0, 256);
    save("hub-resume");
    c = begin(1500);
    pw_view_status(&c, &ui, "10/16 14:05", 80, 0, 256);
    pw_view_hub(&c, &ui, &cat, rows, 5, 256 * 2, 256);
    save("hub-load");
    c = begin(3000);
    pw_view_status(&c, &ui, "10/16 14:05", 20, 1, 256);
    pw_view_hub(&c, &ui, &cat, rows, 5, 256 + 128, 256);
    save("hub-moving");
    rows[2].enabled = 0;
    rows[2].desc = "No saved state";
    c = begin(4500);
    pw_view_status(&c, &ui, "10/16 14:05", 80, 0, 256);
    pw_view_hub(&c, &ui, &cat, rows, 5, 256 * 2, 256);
    save("hub-load-disabled");
    c = begin(6000);
    pw_view_status(&c, &ui, "10/16 14:05", 80, 0, 128);
    pw_view_hub(&c, &ui, &cat, rows, 5, 0, 128);
    pw_draw_lerp(&c, g_game, g_px, 128);
    save("hub-fading-in");
    c = begin(7000);
    pw_view_dialog(&c, &ui, PW_VIEW_CONFIRM, "Do you want to quit the game?", NULL, 0);
    save("dialog-quit");
    c = begin(8000);
    pw_view_dialog(&c, &ui, PW_VIEW_CONFIRM, "Load the saved state?",
                   "Unsaved progress will be lost.", 0);
    save("dialog-confirm");
    c = begin(9000);
    pw_view_dialog(&c, &ui, PW_VIEW_MESSAGE, "The state could not be saved.",
                   "Details are in pocketwatch.log.", 1);
    save("dialog-message");
    c = begin(10000);
    pw_view_busy(&c, &ui, "Saving...", 10000);
    save("busy");
    return 0;
}

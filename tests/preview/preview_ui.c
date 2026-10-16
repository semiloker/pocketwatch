// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Renders every menu screen on the host into PNGs, for reviewing the look without a PSP:
 *
 *   make -C tests preview FONT=/path/to/ltn0.pgf      (FONT optional: built-in 8x8 font)
 *
 * The background is a synthetic "game frame". Output: PNGs in tests/build/preview. Never commit the
 * results when a Sony font was used (docs/assets/references/README.md).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/png.h"
#include "gfx/text.h"
#include "ui/icons.h"
#include "ui/views.h"

#define W      480
#define H      272
#define STRIDE 512

static unsigned int g_bg[STRIDE * H], g_px[STRIDE * H];
static unsigned char g_font_data[4 << 20], g_pool[2][64 * 1024], g_scratch[2][32 * 1024];
static pw_pgf g_pgf;
static pw_font g_title, g_body;

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

/* A busy, colourful stand-in for a frozen game frame, dimmed like the menu's background. */
static void make_background(int dim)
{
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int r = 40 + x * 120 / W + ((x / 24 + y / 24) % 2) * 30, g = 60 + y * 90 / H,
                b = 120 - x * 60 / W + ((x * y) % 97 < 8 ? 80 : 0);

            r = r * dim / 256;
            g = g * dim / 256;
            b = b * dim / 256;
            g_bg[y * STRIDE + x] = PW_RGB(r, g, b);
        }
}

static pw_canvas begin(void)
{
    pw_canvas c = {g_px, W, H, STRIDE};

    memcpy(g_px, g_bg, sizeof(g_px));
    return c;
}

int main(void)
{
    const char *font = getenv("PW_PREVIEW_FONT");
    pw_ui ui;
    pw_canvas c;
    pw_hub_row rows[] = {
        {PW_ICON_RESUME, "Resume Game", "Return to the game.", 1},
        {PW_ICON_SAVE, "Save State", "Saved at 14:05:31 (4.2 s).", 1},
        {PW_ICON_LOAD, "Load State", "Return to the moment saved at 14:05:31.", 1},
        {PW_ICON_QUIT, "Quit Game", "Quit the game and return to the home menu.", 1},
        {PW_ICON_CARD, "Memory Stick Test", "Debug: is the card's view fresh after a load? (T1)",
         1},
    };

    pw_font_init_fallback(&g_title, 2);
    pw_font_init_fallback(&g_body, 1);
    if (font) {
        FILE *f = fopen(font, "rb");
        size_t n = f ? fread(g_font_data, 1, sizeof(g_font_data), f) : 0;

        if (f)
            fclose(f);
        if (n == 0 || pw_pgf_open(&g_pgf, g_font_data, (unsigned int)n) < 0 ||
            pw_font_init(&g_title, &g_pgf, 205, g_pool[0], sizeof(g_pool[0]), g_scratch[0],
                         sizeof(g_scratch[0])) < 0 ||
            pw_font_init(&g_body, &g_pgf, 160, g_pool[1], sizeof(g_pool[1]), g_scratch[1],
                         sizeof(g_scratch[1])) < 0) {
            fprintf(stderr, "%s: not a usable PGF\n", font);
            return 1;
        }
    }
    memset(&ui, 0, sizeof(ui));
    ui.title = &g_title;
    ui.body = &g_body;
    ui.cross_enter = 1;

    make_background(100);
    c = begin();
    pw_view_hub(&c, &ui, rows, 5, 0, 256);
    save("hub-resume");
    c = begin();
    pw_view_hub(&c, &ui, rows, 5, 256 * 2, 256);
    save("hub-load");
    c = begin();
    pw_view_hub(&c, &ui, rows, 5, 256 + 128, 256);
    save("hub-moving");
    rows[2].enabled = 0;
    rows[2].desc = "No state saved yet.";
    c = begin();
    pw_view_hub(&c, &ui, rows, 5, 256 * 2, 256);
    save("hub-load-disabled");
    c = begin();
    pw_view_dialog(&c, &ui, PW_VIEW_CONFIRM, "Load the saved state?",
                   "Unsaved progress will be lost.", 0);
    save("dialog-confirm");
    c = begin();
    pw_view_dialog(&c, &ui, PW_VIEW_MESSAGE, "The state could not be saved.",
                   "Details are in pocketwatch.log.", 1);
    save("dialog-message");
    c = begin();
    pw_view_busy(&c, &ui, "Saving...");
    save("busy");
    return 0;
}

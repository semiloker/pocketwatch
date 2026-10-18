// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * How long one menu frame takes, by part (make -C tests bench [FONT=<a .pgf>]). The host is far
 * faster than the PSP's 333 MHz Allegrex, so only the proportions matter: they show where a
 * frame's time goes. Built without sanitizers, at -O2. With FONT, text is the system font's (as on
 * the PSP), else the 8x8 fallback; Pocketwatch Home's text stamps are timed too (ADR-028).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gfx/pgf.h"
#include "gfx_cpu.h"
#include "home_view.h"

#include "ui/backdrop.h"
#include "ui/i18n.h"
#include "ui/icons.h"
#include "ui/shapes.h"
#include "ui/views.h"
#include "ui/xmbbg.h"

#define W      480
#define H      272
#define STRIDE 512
#define FRAMES 300

static unsigned int g_px[STRIDE * H], g_bg[STRIDE * H], g_game[STRIDE * H];
static double now_us(void)
{
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e6 + t.tv_nsec / 1e3;
}

static pw_font g_title, g_body, g_big;
static pw_pgf g_pgf;

/* The PSP's own scales (home/src/main.c, ui.c): title, body, big. */
static int load_font(const char *path)
{
    static unsigned char pool[3][72 * 1024], scratch[3][64 * 1024];
    static const int scales[3] = {205, 160, 256};
    pw_font *f[3] = {&g_title, &g_body, &g_big};
    FILE *fp = path && path[0] ? fopen(path, "rb") : NULL;
    unsigned char *d;
    long n;

    if (!fp)
        return -1;
    fseek(fp, 0, SEEK_END);
    n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    d = malloc((size_t)n);
    if (!d || fread(d, 1, (size_t)n, fp) != (size_t)n ||
        pw_pgf_open(&g_pgf, d, (unsigned int)n) < 0) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    for (int i = 0; i < 3; i++)
        if (pw_font_init(f[i], &g_pgf, scales[i], pool[i], sizeof(pool[i]), scratch[i],
                         sizeof(scratch[i])) < 0)
            return -1;
    return 0;
}

/* Home's text stamps (gfx_gu.c make_stamp): one string rasterised into a cleared canvas, then
   turned into 8-bit coverage - what a cache miss costs. */
static double stamp_us(pw_font *f, const char *s, int max_w, int fresh)
{
    static unsigned int canvas[512 * 40];
    static unsigned char out[512 * 40];
    double t0 = now_us();
    int w = pw_text_width(f, s), h = f->ascent + f->descent + 7, stride;

    if (max_w > 0 && w > max_w)
        w = max_w;
    stride = (w + 8 + 15) & ~15;
    for (int i = 0; i < 20; i++) {
        pw_canvas c = {canvas, w + 8, h, stride};

        if (fresh) /* the glyphs as the first time: decoded again */
            pw_font_init(f, &g_pgf, f->scale, f->pool, f->pool_size, f->scratch, f->scratch_size);
        memset(canvas, 0, (size_t)(stride * h * 4));
        pw_text_draw_fit(&c, f, 3, f->ascent + 3, s, max_w > 0 ? max_w : 512, PW_RGB(255, 255, 255),
                         256, 0);
        for (int k = 0; k < stride * h; k++)
            out[k] = (unsigned char)(canvas[k] & 0xFF);
    }
    return (now_us() - t0) / 20;
}

int main(void)
{
    static const char *const names[PW_STYLE_COUNT] = {"xmb", "modern", "minimal"};
    pw_canvas page = {g_px, W, H, STRIDE}, bg = {g_bg, W, H, STRIDE}, game = {g_game, W, H, STRIDE};
    static unsigned int skypx[60 * 34];
    pw_image sky = {skypx, 60, 34};
    pw_ui ui;
    pw_hub_cat cat = {NULL, PW_ICON_WATCH, "Game", "Castlevania: The Dracula X Chronicles"};
    pw_hub_row rows[] = {
        {NULL, NULL, PW_ICON_RESUME, "Resume Game", NULL, 1},
        {NULL, NULL, PW_ICON_SAVE, "Save State", "2 of 5 slots used", 1},
        {NULL, NULL, PW_ICON_LOAD, "Load State", "Save State 2, saved at 14:05", 1},
        {NULL, NULL, PW_ICON_GEAR, "Settings", NULL, 1},
        {NULL, NULL, PW_ICON_QUIT, "Quit Game", NULL, 1},
    };

    for (int i = 0; i < 60 * 34; i++)
        skypx[i] = PW_RGB(30 + i % 60, 50, 120 + i / 60);
    for (int i = 0; i < STRIDE * H; i++)
        g_game[i] = PW_RGB(i % 255, (i / 7) % 255, 90);
    {
        static unsigned char icon_cache[64 * 1024];

        pw_icon_cache(icon_cache, sizeof(icon_cache));
    }
    pw_font_init_fallback(&g_title, 2);
    pw_font_init_fallback(&g_body, 1);
    pw_font_init_fallback(&g_big, 2);
    if (load_font(getenv("PW_BENCH_FONT")) == 0)
        printf("text: the system font\n");
    else
        printf("text: the 8x8 fallback (pass FONT=<a .pgf> for the PSP's own)\n");
    memset(&ui, 0, sizeof(ui));
    ui.title = &g_title;
    ui.body = &g_body;
    ui.cross_enter = 1;

    printf("%-8s %10s %10s %10s %10s %10s  (us per frame, %d frames)\n", "style", "copy", "wave",
           "status", "hub", "total", FRAMES);
    for (int s = 0; s < PW_STYLE_COUNT; s++) {
        double t_copy = 0, t_wave = 0, t_status = 0, t_hub = 0, t0;

        ui.style = s;
        pw_backdrop_make(&bg, &game, &sky, s);
        for (int f = 0; f < FRAMES; f++) {
            /* as ui/fb.c and ui/hub.c do it: the moving backdrop renews a quarter of the wave a
               frame; over a still one, the status corner comes from its cache once drawn */
            static unsigned int corner[240 * 40];
            int span = H - PW_XMBBG_WAVE_TOP, band = f % 4;

            t0 = now_us();
            memcpy(g_px, g_bg, sizeof(g_px));
            t_copy += now_us() - t0;
            t0 = now_us();
            if (pw_backdrop_moves(s))
                pw_backdrop_frame_rows(&page, (unsigned int)f * 16, s,
                                       PW_XMBBG_WAVE_TOP + span * band / 4,
                                       PW_XMBBG_WAVE_TOP + span * (band + 1) / 4);
            t_wave += now_us() - t0;
            t0 = now_us();
            if (f == 0 || pw_backdrop_moves(s)) {
                pw_view_status(&page, &ui, "10/16 14:05", 80, 0, 256, 0);
                for (int y = 0; y < 40; y++)
                    memcpy(corner + y * 240, g_px + y * STRIDE + 240, 240 * 4);
            } else {
                for (int y = 0; y < 40; y++)
                    memcpy(g_px + y * STRIDE + 240, corner + y * 240, 240 * 4);
            }
            t_status += now_us() - t0;
            t0 = now_us();
            pw_view_hub(&page, &ui, &cat, rows, 5, (f * 7) % (4 * 256), 256);
            t_hub += now_us() - t0;
        }
        printf("%-8s %10.1f %10.1f %10.1f %10.1f %10.1f\n", names[s], t_copy / FRAMES,
               t_wave / FRAMES, t_status / FRAMES, t_hub / FRAMES,
               (t_copy + t_wave + t_status + t_hub) / FRAMES);
    }
    {
        double t0 = now_us();

        for (int i = 0; i < 20; i++)
            pw_backdrop_make(&bg, &game, &sky, PW_STYLE_MODERN);
        printf("modern backdrop (blur, once per opening): %.1f us\n", (now_us() - t0) / 20);
    }
    if (g_pgf.glyphs) { /* Pocketwatch Home: what a text stamp costs when it has to be made */
        static const char *title = "Castlevania: The Dracula X Chronicles",
                          *meta = "ULUS10277   Played 12 h 30 min   Save states: 3";

        printf("home stamp, title (big): %.1f us, glyphs cached %.1f us\n",
               stamp_us(&g_big, title, 432, 1), stamp_us(&g_big, title, 432, 0));
        printf("home stamp, meta (body): %.1f us, glyphs cached %.1f us\n",
               stamp_us(&g_body, meta, 432, 1), stamp_us(&g_body, meta, 432, 0));
    }
    return 0;
}

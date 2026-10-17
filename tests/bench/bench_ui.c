// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * How long one menu frame takes, by part (make -C tests bench). The host is far faster than the
 * PSP's 333 MHz Allegrex, so only the proportions matter: they show where a frame's time goes.
 * Built without sanitizers, at -O2.
 */
#include <stdio.h>
#include <string.h>
#include <time.h>

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
static pw_font g_title, g_body;

static double now_us(void)
{
    struct timespec t;

    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e6 + t.tv_nsec / 1e3;
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
    pw_font_init_fallback(&g_title, 2);
    pw_font_init_fallback(&g_body, 1);
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
            t0 = now_us();
            memcpy(g_px, g_bg, sizeof(g_px));
            t_copy += now_us() - t0;
            t0 = now_us();
            pw_backdrop_frame(&page, (unsigned int)f * 16, s);
            t_wave += now_us() - t0;
            t0 = now_us();
            pw_view_status(&page, &ui, "10/16 14:05", 80, 0, 256, 0);
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
    return 0;
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspdisplay.h>
#include <pspdisplay_kernel.h>
#include <pspkernel.h>
#include <psputilsforkernel.h>
#include <string.h>

#include "core/log.h"
#include "core/mem.h"
#include "core/platform.h"
#include "core/png.h"
#include "stb_sprintf.h"
#include "ui/fb.h"
#include "ui/backdrop.h"
#include "ui/icons.h"
#include "ui/xmbbg.h"

#define FB_STRIDE   512
#define FB_PIXELS   (FB_STRIDE * PW_FB_H)
#define FB_BYTES    (FB_PIXELS * 4)
#define UNCACHED(p) ((void *)(0xA0000000u | ((unsigned int)(p) & 0x1FFFFFFFu)))
#define ICON_CACHE  (64 * 1024) /* rasterised drawn icons (ui/icons.c) */
#define PAGES       5           /* game, sky, two screen pages, the moving backdrop */

static pw_mem_block g_mem;
static unsigned int *g_game, *g_sky, *g_page[2];
static unsigned int *g_moving; /* g_sky with the wave on it; half the wave renewed each frame */
static int g_half;
static int g_back;
static pw_canvas g_canvas;
static const pw_image *g_sky_img;
static int g_style;

static void make_backdrop(void)
{
    pw_canvas game = {g_game, PW_FB_W, PW_FB_H, FB_STRIDE};
    pw_canvas sky = {g_sky, PW_FB_W, PW_FB_H, FB_STRIDE};
    pw_canvas moving = {g_moving, PW_FB_W, PW_FB_H, FB_STRIDE};

    pw_backdrop_make(&sky, &game, g_sky_img, g_style);
    if (pw_backdrop_moves(g_style)) {
        memcpy(g_moving, g_sky, FB_BYTES);
        pw_backdrop_frame(&moving, 0, g_style);
    }
}

/* Renew one half of the wave in g_moving: the sky under those rows, then the wave at t_ms. Half
   the work per frame, and the crest moves less than a pixel a frame, so no seam shows. */
static void move_backdrop(unsigned int t_ms)
{
    const int mid = (PW_XMBBG_WAVE_TOP + PW_FB_H) / 2;
    int y0 = g_half ? mid : PW_XMBBG_WAVE_TOP, y1 = g_half ? PW_FB_H : mid;
    pw_canvas moving = {g_moving, PW_FB_W, PW_FB_H, FB_STRIDE};

    g_half ^= 1;
    memcpy(g_moving + y0 * FB_STRIDE, g_sky + y0 * FB_STRIDE,
           (unsigned int)(y1 - y0) * FB_STRIDE * 4);
    pw_backdrop_frame_rows(&moving, t_ms, g_style, y0, y1);
}

static void capture(void)
{
    void *addr = NULL;
    int bufw = 0, fmt = 0;
    pw_canvas game = {g_game, PW_FB_W, PW_FB_H, FB_STRIDE};

    sceDisplayGetFrameBuf(&addr, &bufw, &fmt, PSP_DISPLAY_SETBUF_IMMEDIATE);
    if (addr == NULL || bufw < PW_FB_W || fmt < PW_FMT_565 || fmt > PW_FMT_8888)
        pw_draw_fill(&game, 0, 0, PW_FB_W, PW_FB_H, PW_RGB(0, 0, 0)); /* nothing behind us */
    else /* read past the cache: the GE (or the game's CPU code) wrote it */
        pw_draw_from_frame(&game, UNCACHED(addr), bufw, fmt, 256);
    make_backdrop();
}

int pw_fb_open(const pw_image *sky, int style)
{
    int ret =
        pw_mem_alloc_in(&g_mem, PW_MEM_PARTITION_SPARE, "PwScreen", PAGES * FB_BYTES + ICON_CACHE);

    if (ret < 0)
        return ret;
    g_sky_img = sky;
    g_style = style;
    g_game = g_mem.ptr;
    g_sky = g_game + FB_PIXELS;
    g_page[0] = g_sky + FB_PIXELS;
    g_page[1] = g_page[0] + FB_PIXELS;
    g_moving = g_page[1] + FB_PIXELS;
    g_back = 0;
    pw_icon_cache(g_moving + FB_PIXELS, ICON_CACHE);
    capture();
    return 0;
}

pw_canvas *pw_fb_begin(int show, unsigned int t_ms)
{
    g_canvas.px = g_page[g_back];
    g_canvas.w = PW_FB_W;
    g_canvas.h = PW_FB_H;
    g_canvas.stride = FB_STRIDE;
    if (pw_backdrop_moves(g_style)) {
        move_backdrop(t_ms);
        memcpy(g_page[g_back], g_moving, FB_BYTES);
    } else {
        memcpy(g_page[g_back], g_sky, FB_BYTES);
    }
    if (show < 256)
        pw_draw_lerp(&g_canvas, g_game, g_page[g_back], show < 0 ? 0 : show);
    return &g_canvas;
}

void pw_fb_present(void)
{
    sceKernelDcacheWritebackAll(); /* the display reads memory, not our cache */
    sceDisplaySetFrameBufferInternal(0, UNCACHED(g_page[g_back]), FB_STRIDE,
                                     PSP_DISPLAY_PIXEL_FORMAT_8888, PSP_DISPLAY_SETBUF_NEXTFRAME);
    g_back ^= 1;
}

void pw_fb_hide(void)
{
    sceDisplaySetFrameBufferInternal(0, NULL, FB_STRIDE, PSP_DISPLAY_PIXEL_FORMAT_8888,
                                     PSP_DISPLAY_SETBUF_NEXTFRAME);
    sceDisplayWaitVblankStart(); /* the display has stopped reading our pages */
}

void pw_fb_recapture(void)
{
    if (g_game != NULL) {
        pw_icon_cache(g_moving + FB_PIXELS, ICON_CACHE); /* P11 was overwritten by the suspend */
        capture();
    }
}

void pw_fb_set_style(int style)
{
    g_style = style;
    if (g_game != NULL)
        make_backdrop();
}

static void shot_row(void *ctx, unsigned int y, unsigned char *rgb)
{
    const unsigned int *row = (const unsigned int *)ctx + y * FB_STRIDE;

    for (int x = 0; x < PW_FB_W; x++) {
        rgb[3 * x] = (unsigned char)(row[x] & 0xFF);
        rgb[3 * x + 1] = (unsigned char)((row[x] >> 8) & 0xFF);
        rgb[3 * x + 2] = (unsigned char)((row[x] >> 16) & 0xFF);
    }
}

int pw_fb_screenshot(void)
{
    static int n;
    unsigned int cap = pw_png_size(PW_FB_W, PW_FB_H);
    const unsigned int *shown = g_page[g_back ^ 1]; /* the page on the screen now */
    char dir[200], path[256];
    pw_mem_block mem;
    int len, ret = -1;

    if (shown == NULL || pw_log_card_blocked() ||
        pw_mem_alloc(&mem, "PwShot", cap + 1 + 3 * PW_FB_W + 64) < 0)
        return -1;
    if (pw_log_dir(dir, sizeof(dir)) < 0)
        stbsp_snprintf(dir, sizeof(dir), "ms0:/seplugins/pocketwatch/");
    len = pw_png_encode_rgb(PW_FB_W, PW_FB_H, shot_row, (void *)shown,
                            (unsigned char *)mem.ptr + cap, mem.ptr, cap);
    stbsp_snprintf(path, sizeof(path), "%sshot-%03d.png", dir, n++);
    if (len > 0 && pw_file_write(path, mem.ptr, (unsigned int)len, 0) == len)
        ret = 0;
    pw_mem_free(&mem);
    pw_log("fb: screenshot %s %s", path, ret == 0 ? "written" : "FAILED");
    return ret;
}

const unsigned int *pw_fb_game(void)
{
    return g_game;
}

void pw_fb_close(void)
{
    pw_fb_hide();
    pw_icon_cache(NULL, 0);
    pw_mem_free(&g_mem);
    g_game = g_sky = g_page[0] = g_page[1] = g_moving = NULL;
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspdisplay.h>
#include <pspdisplay_kernel.h>
#include <pspkernel.h>
#include <psputilsforkernel.h>
#include <string.h>

#include "core/mem.h"
#include "ui/fb.h"
#include "ui/backdrop.h"

#define FB_STRIDE   512
#define FB_PIXELS   (FB_STRIDE * PW_FB_H)
#define FB_BYTES    (FB_PIXELS * 4)
#define UNCACHED(p) ((void *)(0xA0000000u | ((unsigned int)(p) & 0x1FFFFFFFu)))

static pw_mem_block g_mem;
static unsigned int *g_game, *g_sky, *g_page[2];
static int g_back;
static pw_canvas g_canvas;
static const pw_image *g_sky_img;
static int g_style;

static void make_backdrop(void)
{
    pw_canvas game = {g_game, PW_FB_W, PW_FB_H, FB_STRIDE};
    pw_canvas sky = {g_sky, PW_FB_W, PW_FB_H, FB_STRIDE};

    pw_backdrop_make(&sky, &game, g_sky_img, g_style);
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
    int ret = pw_mem_alloc_in(&g_mem, PW_MEM_PARTITION_SPARE, "PwScreen", 4 * FB_BYTES);

    if (ret < 0)
        return ret;
    g_sky_img = sky;
    g_style = style;
    g_game = g_mem.ptr;
    g_sky = g_game + FB_PIXELS;
    g_page[0] = g_sky + FB_PIXELS;
    g_page[1] = g_page[0] + FB_PIXELS;
    g_back = 0;
    capture();
    return 0;
}

pw_canvas *pw_fb_begin(int show, unsigned int t_ms)
{
    g_canvas.px = g_page[g_back];
    g_canvas.w = PW_FB_W;
    g_canvas.h = PW_FB_H;
    g_canvas.stride = FB_STRIDE;
    memcpy(g_page[g_back], g_sky, FB_BYTES);
    pw_backdrop_frame(&g_canvas, t_ms, g_style);
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
    if (g_game != NULL)
        capture();
}

void pw_fb_set_style(int style)
{
    g_style = style;
    if (g_game != NULL)
        make_backdrop();
}

const unsigned int *pw_fb_game(void)
{
    return g_game;
}

void pw_fb_close(void)
{
    pw_fb_hide();
    pw_mem_free(&g_mem);
    g_game = g_sky = g_page[0] = g_page[1] = NULL;
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspdisplay.h>
#include <pspdisplay_kernel.h>
#include <pspkernel.h>
#include <psputilsforkernel.h>
#include <string.h>

#include "core/mem.h"
#include "ui/fb.h"

#define FB_W        480
#define FB_H        272
#define FB_STRIDE   512
#define FB_BYTES    (FB_STRIDE * FB_H * 4)
#define DIM         96 /* game picture brightness behind the menu, of 256 */
#define UNCACHED(p) ((void *)(0xA0000000u | ((unsigned int)(p) & 0x1FFFFFFFu)))

static pw_mem_block g_mem;
static unsigned int *g_bg, *g_page[2];
static int g_back;
static pw_canvas g_canvas;

static void capture(void)
{
    void *addr = NULL;
    int bufw = 0, fmt = 0;
    pw_canvas bg = {g_bg, FB_W, FB_H, FB_STRIDE};

    sceDisplayGetFrameBuf(&addr, &bufw, &fmt, PSP_DISPLAY_SETBUF_IMMEDIATE);
    if (addr == NULL || bufw < FB_W || fmt < PW_FMT_565 || fmt > PW_FMT_8888) {
        pw_draw_fill(&bg, 0, 0, FB_W, FB_H, PW_RGB(0, 0, 0)); /* nothing to show behind us */
        return;
    }
    /* Read past the cache: the GE (or the game's CPU code) wrote it. */
    pw_draw_from_frame(&bg, UNCACHED(addr), bufw, fmt, DIM);
}

int pw_fb_open(void)
{
    int ret = pw_mem_alloc_in(&g_mem, PW_MEM_PARTITION_SPARE, "PwScreen", 3 * FB_BYTES);

    if (ret < 0)
        return ret;
    g_bg = g_mem.ptr;
    g_page[0] = g_bg + FB_BYTES / 4;
    g_page[1] = g_page[0] + FB_BYTES / 4;
    g_back = 0;
    capture();
    return 0;
}

pw_canvas *pw_fb_begin(void)
{
    memcpy(g_page[g_back], g_bg, FB_BYTES);
    g_canvas.px = g_page[g_back];
    g_canvas.w = FB_W;
    g_canvas.h = FB_H;
    g_canvas.stride = FB_STRIDE;
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
    if (g_bg != NULL)
        capture();
}

void pw_fb_close(void)
{
    pw_fb_hide();
    pw_mem_free(&g_mem);
    g_bg = g_page[0] = g_page[1] = NULL;
}

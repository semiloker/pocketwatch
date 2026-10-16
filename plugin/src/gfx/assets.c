// SPDX-License-Identifier: GPL-3.0-or-later
#include "gfx/assets.h"
#include "core/log.h"
#include "core/mem.h"
#include "core/platform.h"

#define FONT_PATH     "flash0:/font/ltn0.pgf"
#define FONT_FILE_MAX (160 * 1024)
#define POOL_TITLE    (40 * 1024)
#define POOL_BODY     (28 * 1024)
#define SCRATCH_MAX   (24 * 1024)
#define SCALE_TITLE   205 /* of 256: 18 px em -> ~14.4 px */
#define SCALE_BODY    160 /* -> ~11.3 px */

static pw_mem_block g_block;
static pw_pgf g_pgf;
static pw_font g_fonts[PW_FONT_COUNT];
static int g_native;

static void fallback(void)
{
    pw_font_init_fallback(&g_fonts[PW_FONT_TITLE], 2);
    pw_font_init_fallback(&g_fonts[PW_FONT_BODY], 1);
    g_native = 0;
}

int pw_assets_init(void)
{
    int size = pw_file_size(FONT_PATH), ret;
    unsigned char *base;
    unsigned int scratch, need;

    fallback();
    if (size <= 0 || size > FONT_FILE_MAX) {
        pw_log("assets: %s not usable (size %d): built-in font", FONT_PATH, size);
        return -1;
    }
    need = ((unsigned int)size + 63) & ~63u;
    if ((ret = pw_mem_alloc_high(&g_block, PW_MEM_PARTITION_EXTRA, "PwAssets",
                                 need + POOL_TITLE + POOL_BODY + 2 * SCRATCH_MAX)) < 0) {
        pw_log("assets: no memory for the font (0x%08X): built-in font", ret);
        return -1;
    }
    base = g_block.ptr;
    if (pw_file_read(FONT_PATH, base, (unsigned int)size) != size ||
        (ret = pw_pgf_open(&g_pgf, base, (unsigned int)size)) < 0) {
        pw_log("assets: %s could not be read or parsed (%d): built-in font", FONT_PATH, ret);
        pw_mem_free(&g_block);
        return -1;
    }
    scratch = pw_font_scratch_size(&g_pgf);
    if (scratch > SCRATCH_MAX ||
        pw_font_init(&g_fonts[PW_FONT_TITLE], &g_pgf, SCALE_TITLE, base + need, POOL_TITLE,
                     base + need + POOL_TITLE + POOL_BODY, SCRATCH_MAX) < 0 ||
        pw_font_init(&g_fonts[PW_FONT_BODY], &g_pgf, SCALE_BODY, base + need + POOL_TITLE,
                     POOL_BODY, base + need + POOL_TITLE + POOL_BODY + SCRATCH_MAX,
                     SCRATCH_MAX) < 0) {
        pw_log("assets: font glyphs too large (%dx%d): built-in font", g_pgf.max_w, g_pgf.max_h);
        fallback();
        pw_mem_free(&g_block);
        return -1;
    }
    g_native = 1;
    pw_log("assets: %s rev %d, %u glyphs U+%04X-U+%04X, max %dx%d, ascender %d/64, %d.%02d pt at "
           "%d dpi (%d KB at %p)",
           FONT_PATH, g_pgf.revision, g_pgf.glyphs, g_pgf.first, g_pgf.last, g_pgf.max_w,
           g_pgf.max_h, g_pgf.ascender, g_pgf.size_h / 64, (g_pgf.size_h % 64) * 100 / 64,
           g_pgf.res_h / 64, size / 1024, (void *)base);
    return 0;
}

void pw_assets_shutdown(void)
{
    fallback();
    pw_mem_free(&g_block);
}

pw_font *pw_assets_font(int which)
{
    return &g_fonts[which < 0 || which >= PW_FONT_COUNT ? PW_FONT_BODY : which];
}

int pw_assets_native(void)
{
    return g_native;
}

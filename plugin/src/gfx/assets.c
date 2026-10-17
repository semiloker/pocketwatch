// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspkernel.h>
#include <psputilsforkernel.h>
#include <string.h>

#include "core/log.h"
#include "core/mem.h"
#include "core/platform.h"
#include "gfx/assets.h"
#include "gfx/rco.h"
#include "gfx/rlz.h"

#define FONT_PATH   "flash0:/font/ltn0.pgf"
#define ICONS_PATH  "flash0:/vsh/resource/topmenu_icon.rco"
#define FG_PATH     "flash0:/vsh/resource/system_plugin_fg.rco"
#define SKY_PATH    "flash0:/vsh/resource/01-12_03g.bmp" /* the PSP-3000's (VitaXMB, capture) */
#define SKY_PATH2   "flash0:/vsh/resource/01-12.bmp"
#define BLOCK_SIZE  (640 * 1024) /* everything kept for the session, top of P9 */
#define FILE_MAX    (320 * 1024) /* largest resource read whole (topmenu_icon.rco: 227 KB) */
#define TABLES_MAX  (32 * 1024)
#define GIM_MAX     (48 * 1024)
#define POOL_TITLE  (40 * 1024)
#define POOL_BODY   (28 * 1024)
#define SCRATCH_MAX (24 * 1024)
#define SCALE_TITLE 205 /* of 256: 18 px em -> ~14.4 px */
#define SCALE_BODY  160 /* -> ~11.3 px */
#define SKY_PIXELS  (64 * 40)

static const struct {
    int id;
    int fg; /* 0: topmenu_icon.rco, 1: system_plugin_fg.rco */
    const char *label;
} k_art[] = {
    {PW_ART_CAT_GAME, 0, "AO"},
    {PW_ART_UMD, 0, "BH"},
    {PW_ART_UMD_GLOW, 0, "BP"},
    {PW_ART_FOLDER, 0, "BJ"},
    {PW_ART_FOLDER_GLOW, 0, "BR"},
    {PW_ART_UPDATE, 0, "EH"},
    {PW_ART_UPDATE_GLOW, 0, "EP"},
    {PW_ART_MS, 0, "BI"},
    {PW_ART_MS_GLOW, 0, "BQ"},
    {PW_ART_SETTINGS, 0, "BX"},
    {PW_ART_SETTINGS_GLOW, 0, "DH"},
    {PW_ART_BTN_CROSS, 1, "tex_cross"},
    {PW_ART_BTN_CIRCLE, 1, "tex_circle"},
    {PW_ART_BTN_TRIANGLE, 1, "tex_triangle"},
    {PW_ART_BTN_SQUARE, 1, "tex_square"},
    /* system_plugin_fg.rco's battery and topmenu_icon.rco's list focus bar (labels as VitaXMB
       reads them from the PSP's files and from themes) */
    {PW_ART_BATTERY, 1, "tex_battery"},
    {PW_ART_FOCUS, 0, "FL"},
};

static pw_mem_block g_block;
static unsigned char *g_base;
static unsigned int g_used;
static pw_pgf g_pgf;
/* The fonts (glyph slot tables) live in the P9 block with their pools: a state loaded in another
   boot brings back this module's pointers, which must find tables that match the pools. */
static pw_font g_fallback[PW_FONT_COUNT];
static pw_font *g_fonts = g_fallback;
static int g_native;
static pw_image g_art[PW_ART_COUNT];
static int g_have_art[PW_ART_COUNT];
static const unsigned char *g_sky_file;
static unsigned int g_sky_len;
static unsigned int *g_sky_px; /* SKY_PIXELS, in the block (kernel RAM is scarce) */
static pw_image g_sky;
static int g_sky_index = -1;

static void *take(unsigned int n)
{
    n = (n + 63) & ~63u;
    if (g_base == 0 || g_used + n > BLOCK_SIZE)
        return 0;
    g_used += n;
    return g_base + g_used - n;
}

static void fallback_fonts(void)
{
    g_fonts = g_fallback;
    pw_font_init_fallback(&g_fonts[PW_FONT_TITLE], 2);
    pw_font_init_fallback(&g_fonts[PW_FONT_BODY], 1);
    g_native = 0;
}

/* The firmware's inflate for zlib RCO data (2-byte zlib header, raw deflate after it). */
static int inflate(const void *src, unsigned int len, void *dst, unsigned int cap)
{
    const unsigned char *s = src;

    if (len < 2)
        return -1;
    if ((s[0] & 0x0F) == 8 && ((s[0] << 8) | s[1]) % 31 == 0)
        s += 2;
    return sceKernelDeflateDecompress(dst, cap, s, 0);
}

static void load_font(void)
{
    int size = pw_file_size(FONT_PATH), ret;
    unsigned char *file, *pool_t, *pool_b, *scr_t, *scr_b;
    pw_font *fonts;
    unsigned int scratch;

    if (size <= 0 || size > FILE_MAX) {
        pw_log("assets: %s not usable (size %d): built-in font", FONT_PATH, size);
        return;
    }
    file = take((unsigned int)size);
    pool_t = take(POOL_TITLE);
    pool_b = take(POOL_BODY);
    scr_t = take(SCRATCH_MAX);
    scr_b = take(SCRATCH_MAX);
    fonts = take(sizeof(pw_font) * PW_FONT_COUNT);
    if (!file || !pool_t || !pool_b || !scr_t || !scr_b || !fonts) {
        pw_log("assets: no room for the font: built-in font");
        return;
    }
    if (pw_file_read(FONT_PATH, file, (unsigned int)size) != size ||
        (ret = pw_pgf_open(&g_pgf, file, (unsigned int)size)) < 0) {
        pw_log("assets: %s could not be read or parsed: built-in font", FONT_PATH);
        return;
    }
    scratch = pw_font_scratch_size(&g_pgf);
    if (scratch > SCRATCH_MAX ||
        pw_font_init(&fonts[PW_FONT_TITLE], &g_pgf, SCALE_TITLE, pool_t, POOL_TITLE, scr_t,
                     SCRATCH_MAX) < 0 ||
        pw_font_init(&fonts[PW_FONT_BODY], &g_pgf, SCALE_BODY, pool_b, POOL_BODY, scr_b,
                     SCRATCH_MAX) < 0) {
        pw_log("assets: font glyphs too large (%dx%d): built-in font", g_pgf.max_w, g_pgf.max_h);
        fallback_fonts();
        return;
    }
    g_fonts = fonts;
    g_native = 1;
    pw_log("assets: %s rev %d, %u glyphs U+%04X-U+%04X, max %dx%d (%d KB)", FONT_PATH,
           g_pgf.revision, g_pgf.glyphs, g_pgf.first, g_pgf.last, g_pgf.max_w, g_pgf.max_h,
           size / 1024);
}

/* Read one RCO whole into a temporary buffer and decode the images listed for it. */
static void load_rco(const char *path, int fg, unsigned char *file, unsigned char *tables,
                     unsigned char *gim, unsigned char *state, unsigned int *pixels)
{
    int size = pw_file_size(path), found = 0, missing = 0, ret;
    pw_rco r;

    if (size <= 0 || size > FILE_MAX || pw_file_read(path, file, (unsigned int)size) != size) {
        pw_log("assets: %s not readable (size %d): drawn icons", path, size);
        return;
    }
    if ((ret = pw_rco_open(&r, file, (unsigned int)size, tables, TABLES_MAX, state, inflate)) < 0) {
        pw_log("assets: %s: not a usable RCO (%d): drawn icons", path, ret);
        return;
    }
    for (unsigned int i = 0; i < sizeof(k_art) / sizeof(k_art[0]); i++) {
        pw_rco_image im;
        pw_image img;
        int got;

        if (k_art[i].fg != fg)
            continue;
        if (pw_rco_find_image(&r, k_art[i].label, &im) < 0 ||
            (got = pw_rco_read_image(&r, &im, gim, GIM_MAX, state, inflate)) < 0 ||
            pw_gim_decode(gim, (unsigned int)got, pixels, GIM_MAX, &img) < 0) {
            missing++;
            continue;
        }
        unsigned int *keep = take((unsigned int)(img.w * img.h * 4));

        if (keep == 0) {
            missing++;
            continue;
        }
        memcpy(keep, img.px, (unsigned int)(img.w * img.h * 4));
        g_art[k_art[i].id].px = keep;
        g_art[k_art[i].id].w = img.w;
        g_art[k_art[i].id].h = img.h;
        g_have_art[k_art[i].id] = 1;
        found++;
    }
    pw_log("assets: %s: %d image(s) decoded, %d missing (RCO compression %u)", path, found, missing,
           (unsigned int)(file[0x0C] >> 4));
}

static void load_sky(void)
{
    const char *path = SKY_PATH;
    int size = pw_file_size(path);
    unsigned char *keep;

    if (size <= 0) {
        path = SKY_PATH2;
        size = pw_file_size(path);
    }
    if (size <= 0 || size > FILE_MAX || (g_sky_px = take(SKY_PIXELS * 4)) == 0 ||
        (keep = take((unsigned int)size)) == 0 ||
        pw_file_read(path, keep, (unsigned int)size) != size) {
        pw_log("assets: no month sky picture: plain background");
        return;
    }
    g_sky_file = keep;
    g_sky_len = (unsigned int)size;
    pw_log("assets: %s (%d KB)", path, size / 1024);
}

int pw_assets_init(void)
{
    pw_mem_block tmp;
    int ret;

    fallback_fonts();
    if ((ret = pw_mem_alloc_high(&g_block, PW_MEM_PARTITION_EXTRA, "PwAssets", BLOCK_SIZE)) < 0) {
        pw_log("assets: no memory (0x%08X): built-in font, drawn icons", ret);
        return -1;
    }
    g_base = g_block.ptr;
    g_used = 0;
    load_font();
    load_sky();
    /* temporary: whole RCO file, unpacked tables, one GIM, its pixels, RLZ state (P11 at boot) */
    if (pw_mem_alloc(&tmp, "PwAssetsTmp",
                     FILE_MAX + TABLES_MAX + GIM_MAX + GIM_MAX * 4 + PW_RLZ_STATE + 256) >= 0) {
        unsigned char *file = tmp.ptr, *tables = file + FILE_MAX, *gim = tables + TABLES_MAX;
        unsigned int *pixels = (unsigned int *)(void *)(gim + GIM_MAX);
        unsigned char *state = (unsigned char *)(pixels + GIM_MAX);

        load_rco(ICONS_PATH, 0, file, tables, gim, state, pixels);
        load_rco(FG_PATH, 1, file, tables, gim, state, pixels);
        pw_mem_free(&tmp);
    } else {
        pw_log("assets: no scratch memory for the icons: drawn icons");
    }
    pw_log("assets: %u KB of %u KB kept", g_used / 1024, BLOCK_SIZE / 1024);
    return 0;
}

void pw_assets_shutdown(void)
{
    fallback_fonts();
    memset(g_have_art, 0, sizeof(g_have_art));
    g_sky_file = 0;
    g_sky_px = 0;
    g_sky_index = -1;
    g_base = 0;
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

unsigned int pw_assets_base(void)
{
    return (unsigned int)g_base;
}

const pw_image *pw_assets_art(int id)
{
    return id >= 0 && id < PW_ART_COUNT && g_have_art[id] ? &g_art[id] : 0;
}

const pw_image *pw_assets_sky(int index)
{
    if (g_sky_file == 0 || g_sky_px == 0)
        return 0;
    /* Decoded every time (2,000 pixels): a state loaded in another boot brings back this module's
       memory, but the block's contents are this boot's. */
    if (pw_bmp_decode(g_sky_file, g_sky_len, index, g_sky_px, SKY_PIXELS, &g_sky) < 0 &&
        pw_bmp_decode(g_sky_file, g_sky_len, 0, g_sky_px, SKY_PIXELS, &g_sky) < 0)
        return 0;
    g_sky_index = index;
    return &g_sky;
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "gfx/pgf.h"
#include "gfx/text.h"
#include "pwtest.h"
#include "ui/shapes.h"

/* ---- a tiny synthetic PGF (revision 2), written the way the decoder reads it ---- */

typedef struct {
    unsigned char *d;
    unsigned int pos; /* bits */
} bitw;

static void put(bitw *w, unsigned int v, int n)
{
    for (int i = 0; i < n; i++, w->pos++)
        if (v & (1u << i))
            w->d[w->pos >> 3] |= (unsigned char)(1u << (w->pos & 7));
}

static void put32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

static void put16(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
}

static unsigned char g_font[2048];
static unsigned int g_font_size;

/*
 * 'A' (65): 3x2 rows, left 1, top 5, all metrics from the tables, advance 8 px.
 * 'B' (66): 2x3 columns, left 0, top 3, metrics inline, advance 9 px.
 * 'C' (67): in the char map range but no glyph.
 */
static void build_font(void)
{
    unsigned char *d = g_font;
    unsigned int pos = 392, charmap, charptr, data;
    bitw w;

    memset(g_font, 0, sizeof(g_font));
    put16(d + 2, 392);
    memcpy(d + 4, "PGF0", 4);
    put32(d + 0x08, 2);    /* revision */
    put32(d + 0x0C, 6);    /* version */
    put32(d + 0x10, 3);    /* charMapLen */
    put32(d + 0x14, 2);    /* charPtrLen = glyphs */
    put32(d + 0x18, 8);    /* charMapBpe */
    put32(d + 0x1C, 16);   /* charPtrBpe */
    d[0x22] = 4;           /* bpp */
    put32(d + 0x24, 648);  /* 10.125 pt */
    put32(d + 0x2C, 8192); /* 128 dpi */
    put16(d + 0xB6, 65);
    put16(d + 0xB8, 67);
    put32(d + 0xD4, 7 * 64);
    put32(d + 0xD8, (unsigned int)(-2 * 64));
    put16(d + 0xFC, 3);
    put16(d + 0xFE, 3);
    d[0x102] = 1; /* dim */
    d[0x103] = 1; /* xAdj */
    d[0x104] = 1; /* yAdj */
    d[0x105] = 1; /* adv */
    /* tables {H, V}: dim, xAdj, yAdj, adv */
    put32(d + pos + 24, 8 * 64);
    pos += 4 * 8;
    /* shadow map: empty */
    charmap = pos;
    d[charmap + 0] = 0;
    d[charmap + 1] = 1;
    d[charmap + 2] = 0xFF;
    pos += 4;
    charptr = pos;
    pos += 4;
    data = pos;

    /* glyph 0 at offset 0 */
    w.d = d + data;
    w.pos = 0;
    put(&w, 20, 14);
    put(&w, 3, 7);
    put(&w, 2, 7);
    put(&w, 1, 7);
    put(&w, 5, 7);
    put(&w, 0x01 | 0x3C, 6);
    put(&w, 0, 7);
    put(&w, 0, 9);
    for (int t = 0; t < 4; t++)
        put(&w, 0, 8);
    put(&w, 10, 4); /* 6 literals */
    put(&w, 0, 4);
    put(&w, 15, 4);
    put(&w, 7, 4);
    put(&w, 15, 4);
    put(&w, 15, 4);
    put(&w, 0, 4);
    put16(d + charptr, 0);

    /* glyph 1 at the next 32-bit boundary */
    unsigned int g1 = (w.pos + 31) / 32;

    w.pos = g1 * 32;
    put(&w, 40, 14);
    put(&w, 2, 7);
    put(&w, 3, 7);
    put(&w, 0, 7);
    put(&w, 3, 7);
    put(&w, 0x02 | 0x1C, 6); /* columns, adv inline */
    put(&w, 0, 7);
    put(&w, 0, 9);
    for (int t = 0; t < 3; t++)
        put(&w, 0, 8);
    put(&w, 9 * 64, 32);
    put(&w, 0, 32);
    put(&w, 10, 4); /* 6 literals: column 0 = 1 2 3, column 1 = 4 5 6 */
    for (unsigned int v = 1; v <= 6; v++)
        put(&w, v, 4);
    put16(d + charptr + 2, g1);
    g_font_size = data + (w.pos + 7) / 8 + 4;
}

PW_TEST(pgf_decodes_rows_columns_and_metrics)
{
    pw_pgf f;
    pw_pgf_glyph g;
    unsigned char a[64];

    build_font();
    CHECK_EQ(pw_pgf_open(&f, g_font, g_font_size), 0);
    CHECK_EQ(f.revision, 2);
    CHECK_EQ(f.ascender, 7 * 64);

    CHECK_EQ(pw_pgf_render(&f, 'A', &g, a, sizeof(a)), 0);
    CHECK_EQ(g.w, 3);
    CHECK_EQ(g.h, 2);
    CHECK_EQ(g.left, 1);
    CHECK_EQ(g.top, 5);
    CHECK_EQ(g.adv, 8 * 64);
    CHECK_EQ(a[0], 0);
    CHECK_EQ(a[1], 255);
    CHECK_EQ(a[2], 7 * 17);
    CHECK_EQ(a[5], 0);

    CHECK_EQ(pw_pgf_render(&f, 'B', &g, a, sizeof(a)), 0);
    CHECK_EQ(g.adv, 9 * 64);
    CHECK_EQ(a[0], 1 * 17); /* row 0: 1 4 */
    CHECK_EQ(a[1], 4 * 17);
    CHECK_EQ(a[4], 3 * 17); /* row 2: 3 6 */
    CHECK_EQ(a[5], 6 * 17);
}

PW_TEST(pgf_rejects_missing_glyphs_and_damaged_data)
{
    pw_pgf f;
    pw_pgf_glyph g;
    unsigned char a[64];

    build_font();
    CHECK_EQ(pw_pgf_open(&f, g_font, g_font_size), 0);
    CHECK(pw_pgf_render(&f, 'C', &g, a, sizeof(a)) < 0); /* no glyph */
    CHECK(pw_pgf_render(&f, 'Z', &g, a, sizeof(a)) < 0); /* outside the char map */
    CHECK(pw_pgf_render(&f, 'A', &g, a, 4) < 0);         /* scratch too small */

    CHECK_EQ(pw_pgf_open(&f, g_font, f.fontdata_off + 6), 0); /* glyph 1 cut off */
    CHECK(pw_pgf_render(&f, 'B', &g, a, sizeof(a)) < 0);

    g_font[4] = 'X';
    CHECK(pw_pgf_open(&f, g_font, g_font_size) < 0);
    CHECK(pw_pgf_open(&f, g_font, 100) < 0);
}

PW_TEST(text_measures_scales_and_draws)
{
    static unsigned char pool[1024], scratch[2048];
    static unsigned int px[32 * 16];
    pw_canvas c = {px, 32, 16, 32};
    pw_pgf f;
    pw_font font;

    build_font();
    CHECK_EQ(pw_pgf_open(&f, g_font, g_font_size), 0);
    CHECK(pw_font_scratch_size(&f) <= sizeof(scratch));

    CHECK_EQ(pw_font_init(&font, &f, 256, pool, sizeof(pool), scratch, sizeof(scratch)), 0);
    CHECK_EQ(pw_text_width(&font, "AB"), 17);
    CHECK_EQ(pw_text_width(&font, "C"), 0); /* missing, no '?' either */

    CHECK_EQ(pw_font_init(&font, &f, 128, pool, sizeof(pool), scratch, sizeof(scratch)), 0);
    CHECK_EQ(pw_text_width(&font, "AB"), 9); /* 4 + 4.5 px */

    memset(px, 0, sizeof(px));
    CHECK_EQ(pw_font_init(&font, &f, 256, pool, sizeof(pool), scratch, sizeof(scratch)), 0);
    CHECK_EQ(pw_text_draw(&c, &font, 2, 8, "A", 0xFFFFFFFFu, 256, 0), 8);
    /* 'A' bitmap top-left at (2 + 1, 8 - 5): its second pixel is opaque */
    CHECK_EQ(px[3 * 32 + 4], 0xFFFFFFFFu);
    CHECK_EQ(px[3 * 32 + 3], 0);

    pw_font_init_fallback(&font, 2);
    CHECK_EQ(pw_text_width(&font, "ab"), 32);
}

PW_TEST(text_decodes_utf8)
{
    const char *s = "A\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80\xFF";

    CHECK_EQ(pw_utf8_next(&s), 'A');
    CHECK_EQ(pw_utf8_next(&s), 0xE9);
    CHECK_EQ(pw_utf8_next(&s), 0x20AC);
    CHECK_EQ(pw_utf8_next(&s), 0x1F600);
    CHECK_EQ(pw_utf8_next(&s), 0xFFFD);
    CHECK_EQ(*s, 0);
}

PW_TEST(shapes_cover_inside_not_outside)
{
    static unsigned int px[40 * 40];
    pw_canvas c = {px, 40, 40, 40};

    memset(px, 0, sizeof(px));
    pw_shape_disc(&c, PW_FX(20), PW_FX(20), PW_FX(5), 0xFFFFFFFFu, 256);
    CHECK_EQ(px[20 * 40 + 20], 0xFFFFFFFFu);
    CHECK_EQ(px[20 * 40 + 30], 0);

    memset(px, 0, sizeof(px));
    pw_shape_triangle(&c, PW_FX(5), PW_FX(5), PW_FX(35), PW_FX(5), PW_FX(5), PW_FX(35), 0xFFFFFFFFu,
                      256);
    CHECK_EQ(px[10 * 40 + 10], 0xFFFFFFFFu);
    CHECK_EQ(px[34 * 40 + 34], 0);

    memset(px, 0, sizeof(px));
    pw_shape_rrect(&c, PW_FX(4), PW_FX(4), PW_FX(32), PW_FX(32), PW_FX(4), PW_FX(2), 0xFFFFFFFFu,
                   256);
    CHECK_EQ(px[20 * 40 + 4], 0xFFFFFFFFu); /* left edge */
    CHECK_EQ(px[20 * 40 + 20], 0);          /* hollow */
    CHECK_EQ(px[20 * 40 + 1], 0);           /* outside */

    memset(px, 0, sizeof(px));
    pw_shape_ring(&c, PW_FX(20), PW_FX(20), PW_FX(10), PW_FX(2), PW_FX(3), 0xFFFFFFFFu, 256);
    CHECK_EQ(px[30 * 40 + 20], 0xFFFFFFFFu); /* bottom of the ring */
    CHECK_EQ(px[10 * 40 + 20], 0);           /* the gap at the top */
    CHECK_EQ(px[20 * 40 + 20], 0);           /* centre */
}

// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_PGF_H
#define PW_PGF_H

/*
 * Reader for Sony's PGF bitmap fonts (the .pgf files in flash0:/font), so the menu draws text with
 * the system's own font read from the user's PSP (no Sony file ships with Pocketwatch). The format
 * is the one documented by PPSSPP's Core/Font/PGF.cpp (GPL-2.0-or-later) and JPCSP's SceFontInfo
 * (GPL-3.0-or-later); see docs/research/m4-native-look.md. Portable and bounds-checked: a
 * truncated or malformed file makes a call fail, never read outside the buffer.
 */

typedef struct {
    const unsigned char *data;
    unsigned int size;
    int revision;
    unsigned int first, last; /* char code range covered by the char map */
    unsigned int glyphs;      /* charPtrLen */
    unsigned int charmap_bpe, charptr_bpe, shadowmap_bpe;
    unsigned int dim_len, xadj_len, yadj_len, adv_len;
    unsigned int tables_off; /* dim, xAdj, yAdj, adv tables ({s32 H, s32 V} each) */
    unsigned int charmap_off, charptr_off, fontdata_off;
    int ascender, descender; /* 26.6 fixed point */
    int max_w, max_h;        /* largest glyph bitmap, pixels */
    int size_h, res_h;       /* 26.6: points and dpi */
} pw_pgf;

typedef struct {
    int w, h;      /* bitmap size */
    int left, top; /* bitmap origin relative to the pen and the baseline (top grows upwards) */
    int adv;       /* horizontal advance, 26.6 */
} pw_pgf_glyph;

/* Parse the header and locate the tables. Returns 0, or < 0 when this is not a usable PGF. */
int pw_pgf_open(pw_pgf *f, const void *data, unsigned int size);

/* Bytes per glyph bitmap to reserve: the header's maximum plus a margin, since some glyphs (with
   descenders) are larger than the maximum the header states. */
unsigned int pw_pgf_glyph_room(const pw_pgf *f);

/*
 * Render one glyph as 8-bit alpha (w*h bytes, rows). The lower half of `cap` takes the glyph, the
 * upper half is scratch for composed (overlay) glyphs; use 2 * pw_pgf_glyph_room. Returns 0, or
 * < 0 when the font has no such glyph, it does not fit, or its data is damaged.
 */
int pw_pgf_render(const pw_pgf *f, unsigned int code, pw_pgf_glyph *g, unsigned char *alpha,
                  unsigned int cap);

#endif

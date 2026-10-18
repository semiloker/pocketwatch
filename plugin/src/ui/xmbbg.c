// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/xmbbg.h"

/* sin over a quarter turn, 0..64 of 256 steps, scaled by 4096 */
static const short k_sin_q[65] = {
    0,    101,  201,  301,  401,  501,  601,  700,  799,  897,  995,  1092, 1189,
    1285, 1380, 1474, 1567, 1660, 1751, 1842, 1931, 2019, 2106, 2191, 2276, 2359,
    2440, 2520, 2598, 2675, 2751, 2824, 2896, 2967, 3035, 3102, 3166, 3229, 3290,
    3349, 3406, 3461, 3513, 3564, 3612, 3659, 3703, 3745, 3784, 3822, 3857, 3889,
    3920, 3948, 3973, 3996, 4017, 4036, 4052, 4065, 4076, 4085, 4091, 4095, 4096,
};

/* sin of a phase in 1/256 turns, times 4096 */
static int isin(unsigned int phase)
{
    unsigned int p = phase & 255, q = p & 63;

    switch (p >> 6) {
    case 0:
        return k_sin_q[q];
    case 1:
        return k_sin_q[64 - q];
    case 2:
        return -k_sin_q[q];
    default:
        return -k_sin_q[64 - q];
    }
}

PW_PIXEL unsigned int lerp_px(unsigned int a, unsigned int b, int t) /* t 0..256 */
{
    unsigned int rb =
        (((a & 0xFF00FF) * (unsigned int)(256 - t) + (b & 0xFF00FF) * (unsigned int)t) >> 8) &
        0xFF00FF;
    unsigned int g =
        (((a & 0x00FF00) * (unsigned int)(256 - t) + (b & 0x00FF00) * (unsigned int)t) >> 8) &
        0x00FF00;

    return 0xFF000000u | rb | g;
}

void pw_xmbbg_sky(pw_canvas *c, const pw_image *sky)
{
    if (sky == 0 || sky->px == 0 || sky->w < 2 || sky->h < 2) {
        for (int y = 0; y < c->h; y++) { /* no picture: the XMB's own dark grey-blue */
            unsigned int col = PW_RGB(20 + y * 40 / c->h, 22 + y * 44 / c->h, 38 + y * 52 / c->h);

            for (int x = 0; x < c->w; x++)
                c->px[y * c->stride + x] = col;
        }
        return;
    }
    /* Bilinear, pixel centres aligned: source = (dest + 0.5) * sw / dw - 0.5, in 1/256. */
    for (int y = 0; y < c->h; y++) {
        int sy = ((2 * y + 1) * sky->h * 128) / c->h - 128, y0, fy;

        if (sy < 0)
            sy = 0;
        y0 = sy >> 8;
        fy = sy & 255;
        if (y0 >= sky->h - 1) {
            y0 = sky->h - 2;
            fy = 256;
        }
        const unsigned int *r0 = sky->px + y0 * sky->w, *r1 = r0 + sky->w;
        unsigned int *out = c->px + y * c->stride;

        for (int x = 0; x < c->w; x++) {
            int sx = ((2 * x + 1) * sky->w * 128) / c->w - 128, x0, fx;

            if (sx < 0)
                sx = 0;
            x0 = sx >> 8;
            fx = sx & 255;
            if (x0 >= sky->w - 1) {
                x0 = sky->w - 2;
                fx = 256;
            }
            out[x] = lerp_px(lerp_px(r0[x0], r0[x0 + 1], fx), lerp_px(r1[x0], r1[x0 + 1], fx), fy);
        }
    }
}

/* Mix white into a pixel by a (0..256): each channel moves towards 255. */
PW_PIXEL unsigned int lighten(unsigned int p, unsigned int a)
{
    unsigned int rb = p & 0xFF00FF, g = p & 0x00FF00;

    rb += (((0xFF00FF - rb) * a) >> 8) & 0xFF00FF;
    g += (((0x00FF00 - g) * a) >> 8) & 0x00FF00;
    return 0xFF000000u | rb | g;
}

/*
 * The ribbon's brightness by distance from its crest (1/16 px), worked out once: the crest's
 * anti-aliased edge, a bright band, a body fading downwards, and the faint second ribbon above.
 * Per pixel that leaves two table reads and one blend (it covers two thirds of the screen at
 * 60 frames a second).
 */
#define BODY_FROM (-16)
#define BODY_TO   (90 * 16)
#define RIBBON    (10 * 16)

static unsigned char g_body[BODY_TO - BODY_FROM], g_ribbon[RIBBON];
static int g_tables;

static void make_tables(void)
{
    for (int d = BODY_FROM; d < BODY_TO; d++) {
        int a;

        if (d < 0)
            a = (d + 16) * 70 / 16;
        else if (d < 3 * 16)
            a = 70 - d * 30 / (3 * 16);
        else
            a = 40 * (90 * 16 - d) / (87 * 16);
        g_body[d - BODY_FROM] = (unsigned char)(a > 0 ? a : 0);
    }
    for (int r = 0; r < RIBBON; r++)
        g_ribbon[r] = (unsigned char)(22 * (RIBBON - r) / RIBBON);
    g_tables = 1;
}

/*
 * Two slow sines make a ribbon that leans down to the right like the XMB's: a soft crest line,
 * a translucent body fading downwards, and a faint second ribbon above it.
 */
void pw_xmbbg_wave(pw_canvas *c, unsigned int t_ms)
{
    pw_xmbbg_wave_rows(c, t_ms, 0, c->h);
}

void pw_xmbbg_wave_rows(pw_canvas *c, unsigned int t_ms, int y_from, int y_to)
{
    static short top[PW_XMBBG_MAX_W], top2[PW_XMBBG_MAX_W]; /* 1/16 px: < 4352 */
    unsigned int t = t_ms / 16; /* phase drift: ~1/256 turn every 16 ms */
    int bottom = c->h < PW_XMBBG_WAVE_BOTTOM ? c->h : PW_XMBBG_WAVE_BOTTOM, w = c->w,
        y_min = bottom;

    if (!g_tables)
        make_tables();
    if (w > PW_XMBBG_MAX_W)
        w = PW_XMBBG_MAX_W;
    for (int x = 0; x < w; x++) { /* the crest of each column, in 1/16 px */
        top[x] = (short)(PW_XMBBG_WAVE_TOP * 16 + 66 * 16 + x * 16 * 38 / 480 +
                         (isin((unsigned int)(x * 256 / 560) + t / 3) * 14 * 16) / 4096 +
                         (isin((unsigned int)(x * 256 / 250) + 256 - t / 5) * 6 * 16) / 4096);
        top2[x] = (short)(top[x] - 30 * 16 +
                          (isin((unsigned int)(x * 256 / 330) + t / 4 + 80) * 9 * 16) / 4096);
        if (((top2[x] - RIBBON) >> 4) < y_min)
            y_min = (top2[x] - RIBBON) >> 4;
    }
    if (y_min < PW_XMBBG_WAVE_TOP)
        y_min = PW_XMBBG_WAVE_TOP;
    if (y_min < y_from)
        y_min = y_from;
    if (bottom > y_to)
        bottom = y_to;
    /* Row by row: the frame buffer is read and written in memory order, which the PSP's small
       data cache needs (column by column, every pixel was a cache miss). */
    for (int y = y_min; y < bottom; y++) {
        unsigned int *row = c->px + y * c->stride;
        int yc = y * 16 + 8;

        for (int x = 0; x < w; x++) {
            int d = yc - top[x], d2 = yc - top2[x];
            unsigned int a = 0;

            if (d >= BODY_FROM && d < BODY_TO)
                a = g_body[d - BODY_FROM];
            if (d2 > -RIBBON && d2 < RIBBON)
                a += g_ribbon[d2 < 0 ? -d2 : d2];
            if (a)
                row[x] = lighten(row[x], a);
        }
    }
}

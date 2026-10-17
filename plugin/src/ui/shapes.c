// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/shapes.h"

#define ONE  16 /* one pixel in PW_FX units */
#define HALF 8

/*
 * Every shape is rasterised from its exact distance, but only the one-pixel ramp along an edge
 * needs that distance: squared distances decide the pixels that are fully inside or outside, so
 * the square root (and any division) runs for edge pixels only. The menu draws these every frame
 * (docs/ui/styles.md, frame cost in tests/bench).
 */

static unsigned int isqrt(unsigned int v)
{
    unsigned int r = 0, bit = 1u << 30;

    while (bit > v)
        bit >>= 2;
    while (bit) {
        if (v >= r + bit) {
            v -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return r;
}

static unsigned int mix(unsigned int dst, unsigned int src, int a)
{
    unsigned int rb =
        (((dst & 0xFF00FF) * (unsigned int)(256 - a) + (src & 0xFF00FF) * (unsigned int)a) >> 8) &
        0xFF00FF;
    unsigned int g =
        (((dst & 0x00FF00) * (unsigned int)(256 - a) + (src & 0x00FF00) * (unsigned int)a) >> 8) &
        0x00FF00;

    return 0xFF000000u | rb | g;
}

/* Signed distance d (1/16 px, positive inside) to coverage 0-256 with a one-pixel ramp. */
static int coverage(int d)
{
    int v = d + HALF;

    return v <= 0 ? 0 : v >= ONE ? 256 : v * 16;
}

static void plot(pw_canvas *c, int x, int y, unsigned int color, int cov, int alpha)
{
    int a = (cov * alpha) >> 8;

    if (a > 0 && x >= 0 && y >= 0 && x < c->w && y < c->h) {
        unsigned int *p = c->px + y * c->stride + x;

        *p = a >= 256 ? color : mix(*p, color, a);
    }
}

/* Pixel bounds of [lo, hi] in PW_FX, one pixel of margin for the ramp. */
static void span(int lo, int hi, int *a, int *b)
{
    *a = (lo - ONE) / ONE - 1;
    *b = (hi + ONE) / ONE + 1;
}

/* Coverage of a pixel at squared distance d2 from a centre line, for a radius r (all PW_FX). */
static int round_cov(int d2, int r)
{
    int lo = r - HALF, hi = r + HALF;

    if (d2 >= hi * hi)
        return 0;
    if (lo > 0 && d2 <= lo * lo)
        return 256;
    return coverage(r - (int)isqrt((unsigned int)d2));
}

void pw_shape_disc(pw_canvas *c, int cx, int cy, int radius, unsigned int color, int alpha)
{
    int ax, bx, ay, by;

    span(cx - radius, cx + radius, &ax, &bx);
    span(cy - radius, cy + radius, &ay, &by);
    for (int y = ay; y <= by; y++) {
        int dy = y * ONE + HALF - cy;

        for (int x = ax; x <= bx; x++) {
            int dx = x * ONE + HALF - cx;

            plot(c, x, y, color, round_cov(dx * dx + dy * dy, radius), alpha);
        }
    }
}

void pw_shape_capsule(pw_canvas *c, int x0, int y0, int x1, int y1, int radius, unsigned int color,
                      int alpha)
{
    int ex = x1 - x0, ey = y1 - y0, len2 = ex * ex + ey * ey; /* icon strokes: < ~40 px */
    int ax, bx, ay, by;

    if (len2 == 0) {
        pw_shape_disc(c, x0, y0, radius, color, alpha);
        return;
    }
    span((x0 < x1 ? x0 : x1) - radius, (x0 > x1 ? x0 : x1) + radius, &ax, &bx);
    span((y0 < y1 ? y0 : y1) - radius, (y0 > y1 ? y0 : y1) + radius, &ay, &by);
    for (int y = ay; y <= by; y++)
        for (int x = ax; x <= bx; x++) {
            int px = x * ONE + HALF - x0, py = y * ONE + HALF - y0, dot = px * ex + py * ey, t, dx,
                dy;

            t = dot <= 0      ? 0
                : dot >= len2 ? 1024
                              : (int)((unsigned int)dot * 1024u / (unsigned int)len2);
            dx = px - ((ex * t) >> 10);
            dy = py - ((ey * t) >> 10);
            plot(c, x, y, color, round_cov(dx * dx + dy * dy, radius), alpha);
        }
}

void pw_shape_ring(pw_canvas *c, int cx, int cy, int radius, int thick, int gap, unsigned int color,
                   int alpha)
{
    int ax, bx, ay, by, outer = radius + thick / 2;
    int in_r = radius - thick / 2 - HALF, out_r = outer + HALF;
    int in2 = in_r > 0 ? in_r * in_r : -1, out2 = out_r * out_r;

    span(cx - outer, cx + outer, &ax, &bx);
    span(cy - outer, cy + outer, &ay, &by);
    for (int y = ay; y <= by; y++)
        for (int x = ax; x <= bx; x++) {
            int dx = x * ONE + HALF - cx, dy = y * ONE + HALF - cy, d2 = dx * dx + dy * dy;
            int dist, d, cov;

            if (d2 >= out2 || d2 <= in2) /* well outside the band: nothing to draw */
                continue;
            dist = (int)isqrt((unsigned int)d2);
            d = thick / 2 - (dist > radius ? dist - radius : radius - dist);
            cov = coverage(d);
            if (gap > 0 && dy < 0) { /* opening at the top, anti-aliased at its sides */
                int g = coverage((dx < 0 ? -dx : dx) - gap);

                cov = g < cov ? g : cov;
            }
            plot(c, x, y, color, cov, alpha);
        }
}

void pw_shape_rrect(pw_canvas *c, int x, int y, int w, int h, int radius, int thick,
                    unsigned int color, int alpha)
{
    int ax, bx, ay, by, hw = w / 2, hh = h / 2, cx = x + hw, cy = y + hh;

    span(x - thick, x + w + thick, &ax, &bx);
    span(y - thick, y + h + thick, &ay, &by);
    for (int py = ay; py <= by; py++)
        for (int px = ax; px <= bx; px++) {
            int qx = (px * ONE + HALF - cx), qy = (py * ONE + HALF - cy);

            qx = (qx < 0 ? -qx : qx) - (hw - radius);
            qy = (qy < 0 ? -qy : qy) - (hh - radius);
            int mx = qx > 0 ? qx : 0, my = qy > 0 ? qy : 0;
            /* only the corners need a square root: elsewhere one of mx, my is 0 */
            int outside = mx == 0   ? my
                          : my == 0 ? mx
                                    : (int)isqrt((unsigned int)(mx * mx + my * my));
            int inside = qx > qy ? qx : qy;
            int sd = (inside < 0 ? inside : 0) + outside - radius; /* > 0 outside */
            int mid = sd + thick / 2; /* outlines lie inside the rectangle's edge */
            int d = thick == 0 ? -sd : thick / 2 - (mid < 0 ? -mid : mid);

            plot(c, px, py, color, coverage(d), alpha);
        }
}

/* An edge a->b with its length worked out once: p's distance from it, positive on the left. */
typedef struct {
    int ax, ay, ex, ey, len;
} edge_t;

static void edge_init(edge_t *e, int ax, int ay, int bx, int by)
{
    e->ax = ax;
    e->ay = ay;
    e->ex = bx - ax;
    e->ey = by - ay;
    e->len = (int)isqrt((unsigned int)(e->ex * e->ex + e->ey * e->ey));
}

static int edge_dist(const edge_t *e, int px, int py)
{
    return e->len == 0 ? 0 : ((px - e->ax) * e->ey - (py - e->ay) * e->ex) / e->len;
}

void pw_shape_triangle(pw_canvas *c, int x0, int y0, int x1, int y1, int x2, int y2,
                       unsigned int color, int alpha)
{
    int area = (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
    int ax, bx, ay, by, lo, hi;
    edge_t e0, e1, e2;

    if (area < 0) { /* make the winding consistent */
        int tx = x1, ty = y1;

        x1 = x2;
        y1 = y2;
        x2 = tx;
        y2 = ty;
    }
    edge_init(&e0, x0, y0, x1, y1);
    edge_init(&e1, x1, y1, x2, y2);
    edge_init(&e2, x2, y2, x0, y0);
    lo = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
    hi = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
    span(lo, hi, &ax, &bx);
    lo = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
    hi = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
    span(lo, hi, &ay, &by);
    for (int y = ay; y <= by; y++)
        for (int x = ax; x <= bx; x++) {
            int px = x * ONE + HALF, py = y * ONE + HALF;
            int d0 = -edge_dist(&e0, px, py), d1 = -edge_dist(&e1, px, py),
                d2 = -edge_dist(&e2, px, py);
            int d = d0 < d1 ? (d0 < d2 ? d0 : d2) : (d1 < d2 ? d1 : d2);

            plot(c, x, y, color, coverage(d), alpha);
        }
}

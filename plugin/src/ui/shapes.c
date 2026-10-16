// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/shapes.h"

#define ONE  16 /* one pixel in PW_FX units */
#define HALF 8

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
    int a = cov * alpha / 256;

    if (a > 0 && x >= 0 && y >= 0 && x < c->w && y < c->h) {
        unsigned int *p = c->px + y * c->stride + x;

        *p = mix(*p, color, a > 256 ? 256 : a);
    }
}

/* Pixel bounds of [lo, hi] in PW_FX, one pixel of margin for the ramp. */
static void span(int lo, int hi, int *a, int *b)
{
    *a = (lo - ONE) / ONE - 1;
    *b = (hi + ONE) / ONE + 1;
}

void pw_shape_capsule(pw_canvas *c, int x0, int y0, int x1, int y1, int radius, unsigned int color,
                      int alpha)
{
    int ex = x1 - x0, ey = y1 - y0, len2 = ex * ex + ey * ey; /* segments < ~90 px */
    int ax, bx, ay, by;

    span((x0 < x1 ? x0 : x1) - radius, (x0 > x1 ? x0 : x1) + radius, &ax, &bx);
    span((y0 < y1 ? y0 : y1) - radius, (y0 > y1 ? y0 : y1) + radius, &ay, &by);
    for (int y = ay; y <= by; y++)
        for (int x = ax; x <= bx; x++) {
            int px = x * ONE + HALF - x0, py = y * ONE + HALF - y0, t = 0, dx, dy;

            if (len2 > 0) { /* position along the segment, 1/1024 */
                int dot = px * ex + py * ey;

                t = dot <= 0 ? 0 : dot >= len2 ? 1024 : dot * 1024 / len2;
            }
            dx = px - ex * t / 1024;
            dy = py - ey * t / 1024;
            plot(c, x, y, color, coverage(radius - (int)isqrt((unsigned int)(dx * dx + dy * dy))),
                 alpha);
        }
}

void pw_shape_disc(pw_canvas *c, int cx, int cy, int radius, unsigned int color, int alpha)
{
    pw_shape_capsule(c, cx, cy, cx, cy, radius, color, alpha);
}

void pw_shape_ring(pw_canvas *c, int cx, int cy, int radius, int thick, int gap, unsigned int color,
                   int alpha)
{
    int ax, bx, ay, by, outer = radius + thick / 2;

    span(cx - outer, cx + outer, &ax, &bx);
    span(cy - outer, cy + outer, &ay, &by);
    for (int y = ay; y <= by; y++)
        for (int x = ax; x <= bx; x++) {
            int dx = x * ONE + HALF - cx, dy = y * ONE + HALF - cy;
            int dist = (int)isqrt((unsigned int)(dx * dx + dy * dy));
            int d = thick / 2 - (dist > radius ? dist - radius : radius - dist);
            int cov = coverage(d);

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
            int outside = (int)isqrt((unsigned int)(mx * mx + my * my));
            int inside = qx > qy ? qx : qy;
            int sd = (inside < 0 ? inside : 0) + outside - radius; /* > 0 outside */
            int mid = sd + thick / 2; /* outlines lie inside the rectangle's edge */
            int d = thick == 0 ? -sd : thick / 2 - (mid < 0 ? -mid : mid);

            plot(c, px, py, color, coverage(d), alpha);
        }
}

/* Distance (1/16 px) of p from the edge a->b, positive on the left (inside for CCW). */
static int edge(int ax, int ay, int bx, int by, int px, int py)
{
    int ex = bx - ax, ey = by - ay, len = (int)isqrt((unsigned int)(ex * ex + ey * ey));

    if (len == 0)
        return 0;
    return ((px - ax) * ey - (py - ay) * ex) / len;
}

void pw_shape_triangle(pw_canvas *c, int x0, int y0, int x1, int y1, int x2, int y2,
                       unsigned int color, int alpha)
{
    int area = (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
    int ax, bx, ay, by, lo, hi;

    if (area < 0) { /* make the winding consistent */
        int tx = x1, ty = y1;

        x1 = x2;
        y1 = y2;
        x2 = tx;
        y2 = ty;
    }
    lo = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
    hi = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
    span(lo, hi, &ax, &bx);
    lo = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
    hi = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
    span(lo, hi, &ay, &by);
    for (int y = ay; y <= by; y++)
        for (int x = ax; x <= bx; x++) {
            int px = x * ONE + HALF, py = y * ONE + HALF;
            int d0 = -edge(x0, y0, x1, y1, px, py), d1 = -edge(x1, y1, x2, y2, px, py),
                d2 = -edge(x2, y2, x0, y0, px, py);
            int d = d0 < d1 ? (d0 < d2 ? d0 : d2) : (d1 < d2 ? d1 : d2);

            plot(c, x, y, color, coverage(d), alpha);
        }
}

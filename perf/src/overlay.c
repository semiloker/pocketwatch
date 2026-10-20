// SPDX-License-Identifier: GPL-3.0-or-later
#include "overlay.h"

#include "ui/font8x8.h"

/* ---- the lines (no printf in a kernel module: a few appenders) ---- */

typedef struct {
    char *s;
    int n;
} line;

static void put(line *l, const char *t)
{
    while (*t && l->n < PW_PERF_COLS)
        l->s[l->n++] = *t++;
    l->s[l->n] = 0;
}

static void put_int(line *l, int v)
{
    char d[12];
    int k = 0;
    unsigned int u = v < 0 ? (unsigned int)-v : (unsigned int)v;

    do
        d[k++] = (char)('0' + u % 10);
    while ((u /= 10) && k < 11);
    if (v < 0)
        d[k++] = '-';
    while (k > 0 && l->n < PW_PERF_COLS)
        l->s[l->n++] = d[--k];
    l->s[l->n] = 0;
}

/* v / 100 with two decimals ("4.10" for 410) */
static void put_cents(line *l, int v)
{
    put_int(l, v / 100);
    put(l, ".");
    if (v % 100 < 10)
        put(l, "0");
    put_int(l, v % 100);
}

int pw_perf_lines(const pw_perf_stats *s, int mode, char lines[2][PW_PERF_COLS + 1])
{
    line a = {lines[0], 0}, b = {lines[1], 0};

    lines[0][0] = lines[1][0] = 0;
    if (mode == PW_PERF_OFF)
        return 0;
    if (s->fps >= 0) {
        put_int(&a, s->fps);
        put(&a, " FPS");
    } else {
        put(&a, "-- FPS"); /* the game showed no new frame (or always the same one) */
    }
    if (mode == PW_PERF_FPS)
        return 1;
    if (s->cpu_pct != PW_PERF_NA) {
        put(&a, "  CPU ");
        put_int(&a, s->cpu_pct);
        put(&a, "%");
    }
    if (s->cpu_mhz > 0) {
        put(&a, "  ");
        put_int(&a, s->cpu_mhz);
        put(&a, "/");
        put_int(&a, s->bus_mhz);
        put(&a, " MHz");
    }
    if (s->free_kb != PW_PERF_NA) {
        put(&a, "  RAM ");
        put_int(&a, s->free_kb);
        put(&a, " KB free");
    }
    put(&b, "BAT");
    if (s->bat_pct != PW_PERF_NA) {
        put(&b, " ");
        put_int(&b, s->bat_pct);
        put(&b, "%");
    }
    if (s->bat_ma != PW_PERF_NA) {
        put(&b, "  ");
        put_int(&b, s->bat_ma);
        put(&b, " mA");
    }
    if (s->bat_mv != PW_PERF_NA) {
        put(&b, "  ");
        put_cents(&b, s->bat_mv / 10);
        put(&b, " V");
    }
    if (s->bat_temp != PW_PERF_NA) {
        put(&b, "  ");
        put_int(&b, s->bat_temp);
        put(&b, " C");
    }
    if (s->bat_min != PW_PERF_NA && s->bat_min > 0) {
        put(&b, "  ");
        put_int(&b, s->bat_min / 60);
        put(&b, s->bat_min % 60 < 10 ? ":0" : ":");
        put_int(&b, s->bat_min % 60);
        put(&b, " left");
    }
    return 2;
}

/* ---- drawing ---- */

static void dot(void *fb, int stride, int fmt, int x, int y, int white)
{
    static const unsigned short k16[3][2] = {{0x0000, 0xFFFF}, {0x8000, 0xFFFF}, {0xF000, 0xFFFF}};

    if (fmt == 3)
        ((unsigned int *)fb)[y * stride + x] = white ? 0xFFFFFFFFu : 0xFF000000u;
    else if (fmt >= 0 && fmt < 3)
        ((unsigned short *)fb)[y * stride + x] = k16[fmt][white];
}

static int lit(const char *text, int n, int px, int py)
{
    int c;

    if (px < 0 || py < 0 || px >= n * 8 || py >= 8)
        return 0;
    c = (unsigned char)text[px >> 3];
    if (c < 32 || c > 126)
        c = '?';
    return pw_font8x8[c - 32][py] >> (7 - (px & 7)) & 1;
}

void pw_perf_render(const char *text, pw_perf_mask *m)
{
    int n = 0;

    while (text[n] && n < PW_PERF_COLS)
        n++;
    for (int y = 0; y < PW_PERF_H; y++)
        for (int b = 0; b < (PW_PERF_W + 7) / 8; b++)
            m->lit[y][b] = m->edge[y][b] = 0;
    m->w = n * 8 + 2;
    /* mask pixel (x, y) is the font's (x - 1, y - 1) */
    for (int y = 0; y < PW_PERF_H; y++)
        for (int x = 0; x < m->w; x++) {
            int on = lit(text, n, x - 1, y - 1), near = 0;

            for (int dy = -1; dy <= 1 && !on && !near; dy++)
                for (int dx = -1; dx <= 1 && !near; dx++)
                    near = lit(text, n, x - 1 + dx, y - 1 + dy);
            if (on)
                m->lit[y][x >> 3] |= (unsigned char)(0x80 >> (x & 7));
            else if (near)
                m->edge[y][x >> 3] |= (unsigned char)(0x80 >> (x & 7));
        }
}

void pw_perf_draw(void *fb, int stride, int fmt, int w, int h, int x, int y, const pw_perf_mask *m)
{
    for (int r = 0; r < PW_PERF_H; r++) {
        int py = y + r;

        if (py < 0 || py >= h)
            continue;
        for (int b = 0; b < (m->w + 7) / 8; b++) {
            unsigned int on = m->lit[r][b], edge = m->edge[r][b];

            if (!(on | edge))
                continue;
            for (int k = 0; k < 8; k++) {
                int px = x + b * 8 + k;

                if (px < 0 || px >= w || !((on | edge) & (0x80u >> k)))
                    continue;
                dot(fb, stride, fmt, px, py, (on >> (7 - k)) & 1);
            }
        }
    }
}

int pw_perf_next(int mode)
{
    return mode == PW_PERF_OFF ? PW_PERF_FPS : mode == PW_PERF_FPS ? PW_PERF_FULL : PW_PERF_OFF;
}

int pw_perf_parse(const char *text, int len)
{
    for (int i = 0; i + 3 <= len; i++)
        if (text[i] == 'f' && text[i + 1] == 'p' && text[i + 2] == 's')
            return PW_PERF_FPS;
    return PW_PERF_FULL;
}

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

/* colour 0 black, 1 white, 2 grey, in each of the display's formats */
static void dot(void *fb, int stride, int fmt, int x, int y, int colour)
{
    static const unsigned short k16[3][3] = {
        {0x0000, 0xFFFF, 0x4208}, {0x8000, 0xFFFF, 0xA108}, {0xF000, 0xFFFF, 0xF444}};
    static const unsigned int k32[3] = {0xFF000000u, 0xFFFFFFFFu, 0xFF404040u};

    if (fmt == 3)
        ((unsigned int *)fb)[y * stride + x] = k32[colour];
    else if (fmt >= 0 && fmt < 3)
        ((unsigned short *)fb)[y * stride + x] = k16[fmt][colour];
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

/* ---- perf.txt ---- */

/* 1 when s (n bytes, then a space or the end) is the word w. */
static int word_is(const char *s, int n, const char *w)
{
    int i = 0;

    for (; i < n && w[i] && s[i] == w[i]; i++)
        ;
    return !w[i] && (i == n || s[i] == ' ' || s[i] == '\r' || s[i] == '\t');
}

void pw_perf_cfg_parse(const char *text, int len, pw_perf_cfg *c)
{
    c->mode = PW_PERF_FULL;
    c->osd = PW_OSD_HOME;
    c->boost = 0;
    for (int i = 0; i < len;) {
        int e = i, k, v;

        while (e < len && text[e] != '\n')
            e++;
        for (k = i; k < e && text[k] == ' '; k++)
            ;
        for (v = k; v < e && text[v] != '='; v++)
            ;
        if (v < e) {
            const char *val = text + v + 1;
            int n, key = v - k;

            while (key > 0 && text[k + key - 1] == ' ')
                key--;
            while (val < text + e && *val == ' ')
                val++;
            n = (int)(text + e - val);
            if (word_is(text + k, key, "mode"))
                c->mode = word_is(val, n, "off")   ? PW_PERF_OFF
                          : word_is(val, n, "fps") ? PW_PERF_FPS
                                                   : PW_PERF_FULL;
            else if (word_is(text + k, key, "osd"))
                c->osd = word_is(val, n, "off")   ? PW_OSD_OFF
                         : word_is(val, n, "all") ? PW_OSD_ALL
                                                  : PW_OSD_HOME;
            else if (word_is(text + k, key, "boost")) {
                int b = 0, d = 0;

                for (; d < n && d < 3 && val[d] >= '0' && val[d] <= '9'; d++)
                    b = b * 10 + (val[d] - '0');
                c->boost = d && b <= 100 ? b : 0;
            }
        }
        i = e + 1;
    }
}

/* ---- the brightness boost ---- */

int pw_bright_boost(int level, int boost)
{
    int v;

    if (level <= 0 || boost <= 0)
        return level;
    v = level + (100 - level) * (boost > 100 ? 100 : boost) / 100;
    return v > 100 ? 100 : v;
}

void pw_bright_init(pw_bright *b)
{
    b->set = b->base = -1;
    b->fights = 0;
    b->t_set = 0;
}

int pw_bright_step(pw_bright *b, int level, int boost, unsigned int now_ms)
{
    int want;

    if (level < 0 || b->fights >= PW_BRIGHT_FIGHTS)
        return -1;
    if (b->set >= 0 && level == b->set) { /* ours still: the boost changed? */
        want = boost > 0 ? pw_bright_boost(b->base, boost) : b->base;
        if (want == b->set || want <= 0)
            return -1;
        b->set = boost > 0 ? want : -1;
        b->t_set = now_ms;
        return want;
    }
    /* the system's level (or another's) */
    if (b->set >= 0 && now_ms - b->t_set < PW_BRIGHT_FIGHT_MS)
        b->fights++;
    b->base = level;
    b->set = -1;
    want = pw_bright_boost(level, boost);
    if (level == 0 || want == level) /* (the screen off stays off) */
        return -1;
    b->set = want;
    b->t_set = now_ms;
    return want;
}

/* ---- the volume and brightness indicator ---- */

int pw_osd_step(pw_osd *o, int vol, int mute, int bright, unsigned int now_ms)
{
    int kind = PW_OSD_NONE;

    if (o->primed) {
        if (vol != o->vol || mute != o->mute)
            kind = PW_OSD_VOLUME;
        else if (bright != o->bright)
            kind = PW_OSD_BRIGHT;
    }
    o->primed = 1;
    o->vol = vol;
    o->mute = mute;
    o->bright = bright;
    if (kind == PW_OSD_NONE)
        return 0;
    o->kind = kind;
    o->t = now_ms;
    return 1;
}

int pw_osd_shown(const pw_osd *o, unsigned int now_ms)
{
    return o->kind != PW_OSD_NONE && now_ms - o->t < PW_OSD_MS ? o->kind : PW_OSD_NONE;
}

void pw_osd_text(const pw_osd *o, int level, char out[PW_PERF_COLS + 1])
{
    line l = {out, 0};

    out[0] = 0;
    if (o->kind == PW_OSD_VOLUME && o->mute == 1) {
        put(&l, "MUTE");
    } else if (o->kind == PW_OSD_VOLUME) {
        put(&l, "VOL ");
        put_int(&l, o->vol < 0 ? 0 : o->vol);
    } else if (o->kind == PW_OSD_BRIGHT) {
        put(&l, "BRIGHT ");
        put_int(&l, o->bright < 0 ? 0 : o->bright + 1);
        put(&l, "/4");
        if (level > 0) {
            put(&l, " ");
            put_int(&l, level);
            put(&l, "%");
        }
    }
}

static void box(void *fb, int stride, int fmt, int w, int h, int x0, int y0, int bw, int bh,
                int colour)
{
    for (int y = y0 < 0 ? 0 : y0; y < y0 + bh && y < h; y++)
        for (int x = x0 < 0 ? 0 : x0; x < x0 + bw && x < w; x++)
            dot(fb, stride, fmt, x, y, colour);
}

void pw_osd_draw(void *fb, int stride, int fmt, int w, int h, const pw_perf_mask *label, int k,
                 int n)
{
    int x0 = (w - PW_OSD_W) / 2, y0 = h - PW_OSD_H - 16, bx = x0 + 10 + label->w + 10,
        bw = x0 + PW_OSD_W - 10 - bx;

    box(fb, stride, fmt, w, h, x0, y0, PW_OSD_W, PW_OSD_H, 1); /* the edge */
    box(fb, stride, fmt, w, h, x0 + 1, y0 + 1, PW_OSD_W - 2, PW_OSD_H - 2, 0);
    pw_perf_draw(fb, stride, fmt, w, h, x0 + 9, y0 + (PW_OSD_H - PW_PERF_H) / 2, label);
    if (n <= 0 || bw < n * 2)
        return;
    for (int i = 0; i < n; i++) { /* n steps, a pixel apart */
        int sx = bx + i * bw / n, sw = (i + 1) * bw / n - i * bw / n - 1;

        box(fb, stride, fmt, w, h, sx, y0 + 8, sw, PW_OSD_H - 16, i < k ? 1 : 2);
    }
}

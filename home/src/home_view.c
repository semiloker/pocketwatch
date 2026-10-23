// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "stb_sprintf.h"
#include "ui/i18n.h"
#include "ui/shapes.h"
#include "ui/xmbbg.h"
#include "view_internal.h"

/* ---- what the styles share ---- */

unsigned int hm_mix(unsigned int c0, unsigned int c1, int t)
{
    unsigned int out = 0xFF000000u;

    for (int s = 0; s < 24; s += 8)
        out |= (unsigned int)hm_lerp((int)(c0 >> s & 0xFF), (int)(c1 >> s & 0xFF), t) << s;
    return out;
}

void hm_palette_of(const hm_ui *ui, hm_palette *p)
{
    unsigned int acc = ui->accent ? ui->accent : C_ON;

    p->text = C_TEXT;
    p->sub = C_SUB;
    p->dim = C_DIM;
    p->card = C_CARD;
    p->knob = C_KNOB;
    p->sel = C_TEXT;
    p->ink = C_INK;
    p->ink2 = C_INK2;
    p->head = hm_mix(acc, C_TEXT, 110);
    p->card_a = 200;
    p->sel_a = 235;
    p->shade = 0;
    switch (ui->theme) {
    case HM_THEME_GLASS: /* frosted: lighter cards, the chosen row in the accent */
        p->card = PW_RGB(255, 255, 255);
        p->card_a = 34;
        p->sel = acc;
        p->sel_a = 220;
        p->ink = C_TEXT;
        p->ink2 = PW_RGB(225, 232, 245);
        p->knob = PW_RGB(120, 128, 150);
        break;
    case HM_THEME_OLED: /* black, thin and bright */
        p->card = PW_RGB(0, 0, 0);
        p->card_a = 230;
        p->sel = acc;
        p->sel_a = 256;
        p->ink = C_TEXT;
        p->ink2 = PW_RGB(230, 236, 248);
        p->knob = PW_RGB(52, 54, 62);
        p->shade = 120;
        break;
    case HM_THEME_PAPER: /* light: white cards, dark text */
        p->text = PW_RGB(26, 28, 36);
        p->sub = PW_RGB(96, 102, 118);
        p->dim = PW_RGB(44, 48, 60);
        p->card = PW_RGB(250, 250, 252);
        p->card_a = 236;
        p->sel = acc;
        p->sel_a = 240;
        p->ink = C_TEXT;
        p->ink2 = PW_RGB(232, 238, 250);
        p->knob = PW_RGB(196, 200, 210);
        p->head = hm_mix(acc, PW_RGB(0, 0, 0), 60);
        break;
    default:
        break;
    }
}

void hm_text_right(hm_gfx *g, pw_font *f, int x1, int y, const char *s, unsigned int color,
                   int alpha)
{
    if (s)
        g->text(g, f, x1 - pw_text_width(f, s), y, s, 0, color, alpha);
}

void hm_text_center(hm_gfx *g, pw_font *f, int cx, int y, const char *s, int max_w,
                    unsigned int color, int alpha)
{
    int w;

    if (!s)
        return;
    w = pw_text_width(f, s);
    if (max_w > 0 && w > max_w)
        w = max_w;
    g->text(g, f, cx - w / 2, y, s, max_w, color, alpha);
}

void hm_text_wrap(hm_gfx *g, pw_font *f, int x, int y, int lh, int lines, const char *s, int max_w,
                  int center, unsigned int color, int alpha)
{
    char line[160];

    for (int k = 0; s && *s && k < lines; k++) {
        int n, cut = 0;

        if (k < lines - 1) /* the most words that fit */
            for (int i = 0;; i++) {
                if (s[i] == ' ' || !s[i]) {
                    if (i >= (int)sizeof(line))
                        break;
                    memcpy(line, s, (size_t)i);
                    line[i] = 0;
                    if (pw_text_width(f, line) > max_w && cut > 0)
                        break;
                    cut = i;
                    if (!s[i])
                        break;
                }
            }
        n = k < lines - 1 && cut > 0 ? cut : (int)strlen(s);
        if (n >= (int)sizeof(line))
            n = (int)sizeof(line) - 1;
        memcpy(line, s, (size_t)n);
        line[n] = 0;
        if (center)
            hm_text_center(g, f, x, y + k * lh, line, max_w, color, alpha);
        else
            g->text(g, f, x, y + k * lh, line, max_w, color, alpha);
        s += n;
        while (*s == ' ')
            s++;
    }
}

void hm_chevron(hm_gfx *g, int x, int y, int dir, unsigned int color, int alpha)
{
    for (int k = 0; k < 4; k++) {
        int dx = dir > 0 ? k - 2 : 2 - k;

        g->fill(g, x + dx, y - 4 + k, 2, 1, color, alpha);
        g->fill(g, x + dx, y + 4 - k, 2, 1, color, alpha);
    }
    g->fill(g, x + (dir > 0 ? 2 : -2), y, 2, 1, color, alpha);
}

void hm_switch(hm_gfx *g, int x, int y, int on, unsigned int accent, unsigned int knob, int alpha)
{
    g->rrect(g, x, y - 8, 30, 16, 8, 0, on ? accent : knob, alpha);
    g->rrect(g, x + (on ? 16 : 2), y - 6, 12, 12, 6, 0, C_TEXT, alpha);
}

void hm_tile_draw(hm_gfx *g, const unsigned int *icon, int x, int y, int w, int h, int alpha)
{
    if (icon) {
        g->image(g, icon, HM_ICON_W, HM_ICON_H, HM_ICON_W, x, y, w, h, alpha);
    } else { /* no ICON0: a dark card with a disc */
        g->rrect(g, x, y, w, h, h > 40 ? 8 : 5, 0, C_CARD, alpha * 230 / 256);
        g->icon(g, PW_ICON_DISC, x + w / 2, y + h / 2, h / 2, C_KNOB, alpha);
    }
}

void hm_fav_star(hm_gfx *g, int x, int y, int w, int alpha)
{
    int s = w >= 100 ? 18 : 14;

    g->rrect(g, x + w - s - 3, y + 3, s, s, s / 2, 0, PW_RGB(20, 20, 26), alpha * 200 / 256);
    g->icon(g, HM_ICON_STAR, x + w - 3 - s / 2, y + 3 + s / 2, s - 6, PW_RGB(255, 206, 64), alpha);
}

void hm_cover_draw(hm_gfx *g, const unsigned int *cover, int x, int y, int size, int r, int alpha)
{
    if (cover) {
        g->image(g, cover, HM_COVER, HM_COVER, HM_COVER, x, y, size, size, alpha);
        return;
    }
    /* no cover: a dark square, lit from above, with a note */
    g->rrect(g, x, y, size, size, r, 0, PW_RGB(36, 39, 52), alpha);
    g->rrect(g, x, y, size, size / 2, r, 0, C_TEXT, alpha * 12 / 256);
    g->icon(g, HM_ICON_MUSIC, x + size / 2, y + size / 2, size * 2 / 5, PW_RGB(120, 126, 146),
            alpha);
}

void hm_art_draw(hm_gfx *g, const pw_image *img, int cx, int cy, int alpha)
{
    g->image(g, img->px, img->w, img->h, (img->w + 15) & ~15, cx - img->w / 2, cy - img->h / 2,
             img->w, img->h, alpha);
}

void hm_art_fit(hm_gfx *g, const pw_image *img, int cx, int cy, int w, int h, int alpha)
{
    int dw = img->w, dh = img->h;

    if (dw > w) {
        dh = dh * w / dw;
        dw = w;
    }
    if (dh > h) {
        dw = dw * h / dh;
        dh = h;
    }
    g->image(g, img->px, img->w, img->h, (img->w + 15) & ~15, cx - dw / 2, cy - dh / 2, dw, dh,
             alpha);
}

void hm_button_pic(hm_gfx *g, const hm_ui *ui, int which, int cx, int cy, int size,
                   unsigned int color, int alpha)
{
    if (ui->art && which >= 0 && which < 4 && ui->art->btn[which].px)
        hm_art_draw(g, &ui->art->btn[which], cx, cy, alpha);
    else
        g->button(g, which, cx, cy, size, color, alpha);
}

int hm_hints_right(hm_gfx *g, const hm_ui *ui, const hm_view *v, int x1, int y, unsigned int color,
                   int alpha)
{
    int x = x1;
    const char *label[4] = {v->hint_back, v->hint_square, v->hint_options, v->hint_enter};
    int btn[4] = {ui->cross_enter ? PW_BTN_CIRCLE : PW_BTN_CROSS, PW_BTN_SQUARE, PW_BTN_TRIANGLE,
                  ui->cross_enter ? PW_BTN_CROSS : PW_BTN_CIRCLE};

    if (v->hint_tabs && v->depth == 0) {
        x -= pw_text_width(ui->body, v->hint_tabs);
        g->text(g, ui->body, x, y, v->hint_tabs, 0, color, alpha);
        x -= pw_text_width(ui->body, "L  R  ") + 4;
        g->text(g, ui->body, x, y, "L  R", 0, color, alpha);
        x -= 18;
    }
    for (int i = 0; i < 4; i++) {
        if (!label[i])
            continue;
        x -= pw_text_width(ui->body, label[i]);
        g->text(g, ui->body, x, y, label[i], 0, color, alpha);
        g->button(g, btn[i], x - 9, y - 4, 12, color, alpha);
        x -= 26;
    }
    for (int i = 0; i < 2; i++) {
        const char *s = i ? v->hint_ud : v->hint_lr;

        if (!s)
            continue;
        x -= pw_text_width(ui->body, s);
        g->text(g, ui->body, x, y, s, 0, color, alpha);
        g->icon(g, i ? HM_ICON_UD : HM_ICON_LR, x - 9, y - 4, 12, color, alpha);
        x -= 26;
    }
    return x;
}

int hm_row_next(const hm_row *rows, int n, int i, int dir)
{
    for (int k = i + dir; k >= 0 && k < n; k += dir)
        if (rows[k].kind != HM_ROW_HEADER && rows[k].kind != HM_ROW_NOTE)
            return k;
    return i;
}

unsigned int hm_glyph_tint(int glyph, unsigned int accent)
{
    switch (glyph) {
    case HM_ICON_PALETTE:
    case HM_ICON_LAYERS:
        return PW_RGB(140, 82, 220);
    case HM_ICON_GAMES:
        return PW_RGB(46, 112, 230);
    case HM_ICON_MUSIC:
    case HM_ICON_PHOTOS:
        return PW_RGB(232, 70, 110);
    case HM_ICON_SPEAKER:
        return PW_RGB(236, 76, 64);
    case HM_ICON_CLOCK:
        return PW_RGB(96, 104, 128);
    case HM_ICON_GLOBE:
    case HM_ICON_LANGUAGE:
        return PW_RGB(40, 140, 220);
    case HM_ICON_USB:
        return PW_RGB(24, 160, 150);
    case HM_ICON_BATTERY:
        return PW_RGB(52, 176, 90);
    case PW_ICON_CARD:
    case HM_ICON_FOLDER:
        return PW_RGB(236, 150, 40);
    case HM_ICON_USER:
        return PW_RGB(240, 120, 50);
    case PW_ICON_QUIT:
    case HM_ICON_TRASH:
        return PW_RGB(226, 60, 60);
    case HM_ICON_CHIP:
        return PW_RGB(70, 90, 200);
    case HM_ICON_EXIT:
    case HM_ICON_SYSTEM:
    case PW_ICON_GEAR:
    case HM_ICON_SLEEP:
        return PW_RGB(110, 116, 134);
    default:
        return accent;
    }
}

/* ---- a page moving: every operation shifted and faded ---- */

#define SH(g) ((hm_shift *)(g))

static void sh_fill(hm_gfx *g, int x, int y, int w, int h, unsigned int color, int alpha)
{
    SH(g)->in->fill(SH(g)->in, x + SH(g)->dx, y, w, h, color, alpha * SH(g)->alpha / 256);
}
static void sh_rrect(hm_gfx *g, int x, int y, int w, int h, int r, int line, unsigned int color,
                     int alpha)
{
    SH(g)->in->rrect(SH(g)->in, x + SH(g)->dx, y, w, h, r, line, color, alpha * SH(g)->alpha / 256);
}
static void sh_image(hm_gfx *g, const unsigned int *px, int w, int h, int stride, int x, int y,
                     int dw, int dh, int alpha)
{
    SH(g)->in->image(SH(g)->in, px, w, h, stride, x + SH(g)->dx, y, dw, dh,
                     alpha * SH(g)->alpha / 256);
}
static void sh_backdrop(hm_gfx *g, const unsigned int *bg, const unsigned int *prev, int mix)
{
    SH(g)->in->backdrop(SH(g)->in, bg, prev, mix);
}
static void sh_text(hm_gfx *g, pw_font *f, int x, int y, const char *s, int max_w,
                    unsigned int color, int alpha)
{
    SH(g)->in->text(SH(g)->in, f, x + SH(g)->dx, y, s, max_w, color, alpha * SH(g)->alpha / 256);
}
static void sh_wave(hm_gfx *g, unsigned int t_ms, int alpha)
{
    SH(g)->in->wave(SH(g)->in, t_ms, alpha);
}
static void sh_icon(hm_gfx *g, int icon, int cx, int cy, int size, unsigned int color, int alpha)
{
    SH(g)->in->icon(SH(g)->in, icon, cx + SH(g)->dx, cy, size, color, alpha * SH(g)->alpha / 256);
}
static void sh_button(hm_gfx *g, int button, int cx, int cy, int size, unsigned int color,
                      int alpha)
{
    SH(g)->in->button(SH(g)->in, button, cx + SH(g)->dx, cy, size, color,
                      alpha * SH(g)->alpha / 256);
}
static int sh_warm(hm_gfx *g, pw_font *f, const char *s, int max_w)
{
    return SH(g)->in->warm(SH(g)->in, f, s, max_w);
}

hm_gfx *hm_shift_init(hm_shift *s, hm_gfx *in, int dx, int alpha)
{
    s->ops.fill = sh_fill;
    s->ops.rrect = sh_rrect;
    s->ops.image = sh_image;
    s->ops.backdrop = sh_backdrop;
    s->ops.text = sh_text;
    s->ops.wave = sh_wave;
    s->ops.icon = sh_icon;
    s->ops.button = sh_button;
    s->ops.warm = sh_warm;
    s->in = in;
    s->dx = dx;
    s->alpha = hm_clampi(alpha, 0, 256);
    return &s->ops;
}

/* ---- a photo whole (any style): the picture alone, its name and the buttons for a moment ---- */

static void viewer(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    const char *label[3] = {v->viewer_edit, v->viewer_slideshow, v->viewer_wallpaper};
    int btn[3] = {ui->cross_enter ? PW_BTN_CROSS : PW_BTN_CIRCLE, PW_BTN_TRIANGLE, PW_BTN_SQUARE};

    if (v->editor > 0) {
        hm_draw_editor(g, ui, v);
        return;
    }
    if (v->viewer_caption && v->caption > 0) {
        int w = pw_text_width(ui->body, v->viewer_caption) + 28;

        w = w > HM_W / 2 - 12 ? HM_W / 2 - 12 : w;
        g->rrect(g, 12, HM_H - 36, w, 24, 12, 0, C_CARD, v->caption * 200 / 256);
        g->text(g, ui->body, 26, HM_H - 20, v->viewer_caption, w - 28, C_TEXT, v->caption);
    }
    if (v->caption > 0) { /* what the buttons do, bottom right, on a pill of their own */
        int w = 10, x;

        for (int i = 0; i < 3; i++)
            if (label[i])
                w += 15 + pw_text_width(ui->body, label[i]) + 12;
        if (w <= 10)
            return;
        x = HM_W - 12 - w + 10;
        g->rrect(g, HM_W - 12 - w, HM_H - 36, w, 24, 12, 0, C_CARD, v->caption * 200 / 256);
        for (int i = 0; i < 3; i++) {
            if (!label[i])
                continue;
            g->button(g, btn[i], x + 6, HM_H - 24, 12, C_TEXT, v->caption);
            g->text(g, ui->body, x + 15, HM_H - 20, label[i], 0, C_TEXT, v->caption);
            x += 15 + pw_text_width(ui->body, label[i]) + 12;
        }
    }
}

/* The editor: the picture (the background), a bar of tools along the bottom, the chosen one
   named above it with its value. */
void hm_draw_editor(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    int p = hm_clampi(v->editor, 0, 256), n = v->ed_count, pitch = 40,
        x0 = HM_W / 2 - (n * pitch) / 2, y = HM_H - 34 + (256 - p) * 30 / 256;
    unsigned int acc = ui->accent ? ui->accent : C_ON;

    g->rrect(g, x0 - 10, y - 20, n * pitch + 20, 40, 20, 0, C_CARD, p * 215 / 256);
    g->rrect(g, x0 + v->ed_pos * pitch / 256 + 4, y - 16, pitch - 8, 32, 12, 0, acc, p);
    for (int i = 0; i < n; i++)
        g->icon(g, v->ed_glyphs ? v->ed_glyphs[i] : HM_ICON_SLIDERS, x0 + i * pitch + pitch / 2, y,
                18, C_TEXT, p * (i == v->ed_sel ? 256 : 190) / 256);
    if (v->ed_sel >= 0 && v->ed_sel < n && v->ed_tools[v->ed_sel]) {
        const char *name = v->ed_tools[v->ed_sel];
        int w = pw_text_width(ui->title, name) +
                (v->ed_value ? pw_text_width(ui->body, v->ed_value) + 16 : 0) + 28;

        g->rrect(g, HM_W / 2 - w / 2, y - 50, w, 24, 12, 0, C_CARD, p * 215 / 256);
        g->text(g, ui->title, HM_W / 2 - w / 2 + 14, y - 33, name, 0, C_TEXT, p);
        if (v->ed_value)
            hm_text_right(g, ui->body, HM_W / 2 + w / 2 - 14, y - 34, v->ed_value,
                          hm_mix(acc, C_TEXT, 120), p);
    }
    if (v->ed_hint)
        hm_text_center(g, ui->body, HM_W / 2, 20, v->ed_hint, HM_W - 40, C_TEXT, p * 200 / 256);
}

void hm_draw_toast(hm_gfx *g, const hm_ui *ui, const hm_view *v, unsigned int card,
                   unsigned int text)
{
    int a = hm_clampi(v->toast, 0, 256), w;

    if (a <= 0 || !v->toast_text)
        return;
    w = pw_text_width(ui->body, v->toast_text) + 32;
    g->rrect(g, HM_W / 2 - w / 2, HM_H - 58 + (256 - a) * 8 / 256, w, 26, 13, 0, card,
             a * 230 / 256);
    hm_text_center(g, ui->body, HM_W / 2, HM_H - 41 + (256 - a) * 8 / 256, v->toast_text, 0, text,
                   a);
}

/* ---- the keyboard (HOM-REQ-030) ---- */

#define KB_UNIT  20 /* px of half a key */
#define KB_TOP   72 /* the first row of keys */
#define KB_PITCH 36
#define KB_H     32
#define KB_FIELD (HM_W - 60) /* px of text in the field */

/* The width of the first n characters of s. */
static int kb_width(pw_font *f, const char *s, int n)
{
    char part[HM_KB_BYTES];
    int b = 0;

    for (int i = 0; i < n && s[b]; i++)
        do
            b++;
        while ((s[b] & 0xC0) == 0x80);
    memcpy(part, s, (size_t)b);
    part[b] = 0;
    return pw_text_width(f, part);
}

static void kb_hint(hm_gfx *g, const hm_ui *ui, int *x, int btn, const char *keys,
                    const char *label, unsigned int color, int a)
{
    if (btn >= 0) {
        g->button(g, btn, *x + 5, 260, 10, color, a);
        *x += 14;
    } else {
        g->text(g, ui->body, *x, 264, keys, 0, color, a);
        *x += pw_text_width(ui->body, keys) + 4;
    }
    g->text(g, ui->body, *x, 264, label, 0, color, a);
    *x += pw_text_width(ui->body, label) + 16;
}

void hm_draw_keyboard(hm_gfx *g, const hm_ui *ui, const hm_view *v, const hm_palette *p)
{
    static const char *const page_name[HM_KB_PAGES] = {"АБВ", "#+=", "ABC"}; /* the next one */
    static const hm_palette dark = {.text = C_TEXT,
                                    .sub = C_SUB,
                                    .dim = C_DIM,
                                    .card = C_CARD,
                                    .knob = C_KNOB,
                                    .sel = C_TEXT,
                                    .ink = C_INK,
                                    .ink2 = C_INK2,
                                    .head = C_SUB,
                                    .card_a = 256,
                                    .sel_a = 256};
    const hm_kb *k = v->kb;
    int a = hm_clampi(v->kb_t, 0, 256), dy = (256 - a) * 40 / 256, skip = 0, x;
    char c[16];

    if (!k || a <= 0)
        return;
    if (!p || p->card_a < 128) /* (glass cards: the text would be white on white) */
        p = &dark;
    g->fill(g, 0, 0, HM_W, HM_H, p->card, a);
    /* the name of what is typed, how much is left, the text and its caret */
    g->text(g, ui->body, 24, 20 + dy, k->title ? k->title : "", HM_W / 2, p->sub, a);
    stbsp_snprintf(c, sizeof(c), "%d/%d", hm_kb_chars(k->text), k->max_chars);
    hm_text_right(g, ui->body, HM_W - 24, 20 + dy, c, p->sub, a);
    g->rrect(g, 20, 28 + dy, HM_W - 40, 34, 8, 0, p->knob, a * 200 / 256);
    while (skip < k->caret &&
           kb_width(ui->title, k->text, k->caret) - kb_width(ui->title, k->text, skip) >
               KB_FIELD) /* a long text: its end, where the caret is */
        skip++;
    {
        const char *s = k->text;

        for (int i = 0; i < skip; i++)
            do
                s++;
            while ((*s & 0xC0) == 0x80);
        g->text(g, ui->title, 30, 51 + dy, s, KB_FIELD, p->text, a);
        x = 30 + kb_width(ui->title, s, k->caret - skip);
        if (!k->ask && v->t_ms % 1000 < 600)
            g->fill(g, x, 34 + dy, 2, 22, p->text, a);
    }
    /* the keys */
    for (int r = 0; r < HM_KB_ROWS; r++)
        for (int i = 0; i < hm_kb_keys(k, r); i++) {
            int kx, kw, on = r == k->row && i == k->col, y = KB_TOP + r * KB_PITCH + dy, kind;
            const char *s;
            pw_font *f = ui->title;

            hm_kb_place(k, r, i, &kx, &kw);
            kx = HM_W / 2 + kx * KB_UNIT + 2;
            kw = kw * KB_UNIT - 4;
            kind = hm_kb_special(k, r, i);
            s = kind == HM_KB_CHAR    ? hm_kb_char(k, r, i, c)
                : kind == HM_KB_SHIFT ? "Aa"
                : kind == HM_KB_PAGE  ? page_name[k->page]
                : kind == HM_KB_SPACE ? pw_tr(PW_S_KB_SPACE)
                : kind == HM_KB_ERASE ? pw_tr(PW_S_KB_ERASE)
                                      : pw_tr(PW_S_KB_DONE);
            if (kind != HM_KB_CHAR)
                f = ui->body;
            g->rrect(g, kx, y, kw, KB_H, 6, 0, on ? p->sel : p->knob,
                     a *
                         (on                   ? 256
                          : kind == HM_KB_CHAR ? 150
                                               : 210) /
                         256);
            if (kind == HM_KB_SHIFT && k->shift)
                g->rrect(g, kx, y, kw, KB_H, 6, PW_FX(2), on ? p->ink : p->text, a);
            hm_text_center(g, f, kx + kw / 2, y + KB_H / 2 + (f == ui->title ? 6 : 4), s, kw - 4,
                           on ? p->ink : p->text, a);
        }
    /* what the other buttons do (the PSP's own keyboard: Square erases, Triangle a space) */
    {
        int w = 0, x0;
        const char *lab[4] = {pw_tr(PW_S_KB_ERASE), pw_tr(PW_S_KB_SPACE), pw_tr(PW_S_KB_CURSOR),
                              pw_tr(PW_S_KB_DONE)};

        for (int i = 0; i < 4; i++)
            w += pw_text_width(ui->body, lab[i]) + 16 +
                 (i < 2 ? 14 : pw_text_width(ui->body, i == 2 ? "L R" : "START") + 4);
        x0 = HM_W / 2 - (w - 16) / 2;
        kb_hint(g, ui, &x0, PW_BTN_SQUARE, 0, lab[0], p->sub, a);
        kb_hint(g, ui, &x0, PW_BTN_TRIANGLE, 0, lab[1], p->sub, a);
        kb_hint(g, ui, &x0, -1, "L R", lab[2], p->sub, a);
        kb_hint(g, ui, &x0, -1, "START", lab[3], p->sub, a);
    }
    /* Back with changes: keep them, drop them, or go on typing */
    if (k->ask) {
        const char *opt[3] = {pw_tr(PW_S_KB_SAVE), pw_tr(PW_S_KB_DISCARD), pw_tr(PW_S_KB_KEEP)};
        int w = 0, ox;

        g->fill(g, 0, 0, HM_W, HM_H, C_BLACK, a * 120 / 256);
        g->rrect(g, 50, 88, HM_W - 100, 104, 12, 0, p->card, a);
        g->rrect(g, 50, 88, HM_W - 100, 104, 12, PW_FX(1), p->sub, a * 120 / 256);
        hm_text_center(g, ui->title, HM_W / 2, 124, pw_tr(PW_S_KB_ASK), HM_W - 120, p->text, a);
        for (int i = 0; i < 3; i++)
            w += pw_text_width(ui->body, opt[i]) + 28 + (i ? 10 : 0);
        ox = HM_W / 2 - w / 2;
        for (int i = 0; i < 3; i++) {
            int ow = pw_text_width(ui->body, opt[i]) + 28, on = i == k->ask_sel;

            g->rrect(g, ox, 146, ow, 28, 14, 0, on ? p->sel : p->knob, a * (on ? 256 : 200) / 256);
            hm_text_center(g, ui->body, ox + ow / 2, 164, opt[i], 0, on ? p->ink : p->text, a);
            ox += ow + 10;
        }
    }
}

/* sin of a phase in 1/256 turns, times 256 */
static int isin(unsigned int ph)
{
    static const unsigned short q[65] = {
        0,   6,   13,  19,  25,  31,  38,  44,  50,  56,  62,  68,  74,  80,  86,  92,  98,
        104, 109, 115, 121, 126, 132, 137, 142, 147, 152, 157, 162, 167, 172, 177, 181, 185,
        190, 194, 198, 202, 206, 209, 213, 216, 220, 223, 226, 229, 231, 234, 237, 239, 241,
        243, 245, 247, 248, 250, 251, 252, 253, 254, 255, 255, 256, 256, 256};
    unsigned int p = ph & 255, k = p & 63;
    int v = p < 64 ? q[k] : p < 128 ? q[64 - k] : p < 192 ? -q[k] : -q[64 - k];

    return p == 64 ? 256 : p == 192 ? -256 : v;
}

/* The aurora: three soft lights of the accent's colours drifting slowly (Lissajous paths). */
static void aurora(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    static const short speed[3][2] = {{37, 29}, {23, 41}, {31, 19}};
    int a = hm_clampi(v->aurora, 0, 256);

    for (int k = 0; k < 3; k++) {
        unsigned int t = v->t_ms / 100; /* 1/256 turn every speed x 100 ms */
        int size = 300 + 40 * k,
            x = 240 + 160 * isin(t * 8 / speed[k][0] + (unsigned int)k * 85) / 256,
            y = 136 + 90 * isin(t * 8 / speed[k][1] + (unsigned int)k * 50 + 64) / 256;

        if (ui->glow[k])
            g->image(g, ui->glow[k], HM_GLOW, HM_GLOW, HM_GLOW, x - size / 2, y - size / 2, size,
                     size, a * 180 / 256);
    }
}

void hm_make_glow(unsigned int c, unsigned int *out)
{
    for (int y = 0; y < HM_GLOW; y++)
        for (int x = 0; x < HM_GLOW; x++) {
            int dx = 2 * x - HM_GLOW + 1, dy = 2 * y - HM_GLOW + 1,
                r2 = (dx * dx + dy * dy) * 256 / (HM_GLOW * HM_GLOW), k = 256 - r2,
                al = k > 0 ? k * k / 256 : 0; /* (1 - r^2)^2: soft to the edge */

            out[y * HM_GLOW + x] = (c & 0xFFFFFFu) | (unsigned int)(al * 230 / 256) << 24;
        }
}

/* The performance overlay (ADR-043): a card at the top left, over everything - the frame rate
   big, each value under it with its name (the owner found two lines of numbers "terrible, not
   understandable", 2026-10-20). */
static void perf_overlay(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    int w = 200, h = 34 + 15 * v->perf_n + 4, x = 6, y = 6, fw;

    g->rrect(g, x, y, w, h, 8, 0, C_BLACK, 232);
    g->text(g, ui->title, x + 10, y + 25, v->perf_fps, 80, C_TEXT, 256);
    fw = pw_text_width(ui->title, v->perf_fps);
    g->text(g, ui->body, x + 14 + fw, y + 25, "FPS", 60, C_SUB, 256);
    for (int i = 0; i < v->perf_n; i++) {
        int ry = y + 34 + 15 * i + 10;

        g->text(g, ui->body, x + 10, ry, v->perf_label[i] ? v->perf_label[i] : "", 96, C_SUB, 256);
        hm_text_right(g, ui->body, x + w - 10, ry, v->perf_value[i] ? v->perf_value[i] : "", C_TEXT,
                      256);
    }
}

/* The volume and brightness indicator (HOM-REQ-034): a pill at the bottom middle, over
   everything but the overlay. */
static void osd(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    int a = hm_clampi(v->osd, 0, 256), w = 250, h = 30, x = (HM_W - w) / 2, y = HM_H - 76, tw,
        bx = x + 40, bw, lit;
    unsigned int acc = ui->accent ? ui->accent : C_ON;

    if (a <= 0 || !v->osd_text)
        return;
    tw = pw_text_width(ui->body, v->osd_text);
    bw = w - 40 - 16 - tw - 12;
    lit = bw * hm_clampi(v->osd_level, 0, 256) / 256;
    g->rrect(g, x, y, w, h, h / 2, 0, C_BLACK, a * 215 / 256);
    g->icon(g, v->osd_glyph, x + 22, y + h / 2, 16, C_TEXT, a);
    g->rrect(g, bx, y + h / 2 - 2, bw, 4, 2, 0, C_KNOB, a);
    if (lit > 0)
        g->rrect(g, bx, y + h / 2 - 2, lit, 4, 2, 0, acc, a);
    hm_text_right(g, ui->body, x + w - 16, y + h / 2 + 5, v->osd_text, C_TEXT, a);
}

/* ---- the battery's calibration (BAT-REQ-011): a screen of its own ---- */

#define CAL_CX 24 /* the chart */
#define CAL_CY 82
#define CAL_CW 250
#define CAL_CH 100
#define CAL_RX 292 /* the column of numbers */

static void cal_chart(hm_gfx *g, const hm_ui *ui, const hm_calview *c, const hm_palette *p,
                      unsigned int acc, int a)
{
    g->rrect(g, CAL_CX - 6, CAL_CY - 6, CAL_CW + 12, CAL_CH + 12, 8, 0, p->knob, a * 70 / 256);
    if (c->npct >= 2)
        for (int q = 1; q < 4; q++) /* 25, 50, 75 % */
            g->fill(g, CAL_CX, CAL_CY + CAL_CH * q / 4, CAL_CW, 1, p->sub, a * 50 / 256);
    if (c->npct < 2) {
        if (c->chart_empty)
            hm_text_wrap(g, ui->body, CAL_CX + CAL_CW / 2, CAL_CY + CAL_CH / 2, 16, 2,
                         c->chart_empty, CAL_CW - 20, 1, p->sub, a);
        return;
    }
    for (int i = 0; i < c->npct; i++) { /* a column per point: the charge filled under its top */
        int x0 = CAL_CX + i * CAL_CW / c->npct, x1 = CAL_CX + (i + 1) * CAL_CW / c->npct,
            top = CAL_CY + CAL_CH - CAL_CH * hm_clampi(c->pct[i], 0, 100) / 100;

        if (x1 <= x0)
            continue;
        g->fill(g, x0, top, x1 - x0, CAL_CY + CAL_CH - top, acc, a * 80 / 256);
        g->fill(g, x0, top, x1 - x0, 2, acc, a);
    }
    if (c->chart_left)
        g->text(g, ui->body, CAL_CX, CAL_CY + CAL_CH + 20, c->chart_left, CAL_CW / 2, p->sub, a);
    if (c->chart_right)
        hm_text_right(g, ui->body, CAL_CX + CAL_CW, CAL_CY + CAL_CH + 20, c->chart_right, p->sub,
                      a);
}

static void cal_draw(hm_gfx *g, const hm_ui *ui, const hm_view *v, int a)
{
    static const hm_palette dark = {.text = C_TEXT,
                                    .sub = C_SUB,
                                    .dim = C_DIM,
                                    .card = C_CARD,
                                    .knob = C_KNOB,
                                    .sel = C_TEXT,
                                    .ink = C_INK,
                                    .ink2 = C_INK2,
                                    .head = C_SUB,
                                    .card_a = 256,
                                    .sel_a = 256};
    const hm_calview *c = v->cal;
    unsigned int acc = ui->accent ? ui->accent : C_ON;
    hm_palette pal;
    const hm_palette *p = &pal;
    int x = 24;

    hm_palette_of(ui, &pal);
    if (pal.card_a < 128) /* (glass cards: the text would be white on white) */
        p = &dark;
    g->fill(g, 0, 0, HM_W, HM_H, p->card, a);
    g->text(g, ui->title, 24, 34, c->title ? c->title : "", HM_W - 48, p->text, a);
    for (int i = 0; i < 4; i++) { /* the steps: done, now, to come */
        char t[48];
        int now = c->step == i + 1, done = c->step > i + 1, w;

        stbsp_snprintf(t, sizeof(t), "%d  %s", i + 1, c->steps[i] ? c->steps[i] : "");
        w = pw_text_width(ui->body, t) + 20;
        g->rrect(g, x, 46, w, 22, 11, now || done ? 0 : PW_FX(1), now ? acc : p->knob,
                 a *
                     (now    ? 256
                      : done ? 150
                             : 110) /
                     256);
        hm_text_center(g, ui->body, x + w / 2, 61, t, w - 8,
                       now    ? C_TEXT
                       : done ? p->text
                              : p->sub,
                       a);
        x += w + 8;
    }
    cal_chart(g, ui, c, p, acc, a);
    if (c->big)
        g->text(g, ui->big, CAL_RX, 112, c->big, HM_W - CAL_RX - 24, p->text, a);
    if (c->big_sub)
        g->text(g, ui->body, CAL_RX, 132, c->big_sub, HM_W - CAL_RX - 24, p->sub, a);
    for (int i = 0; i < c->nstats && i < 4; i++) { /* (four: the words under them) */
        const char *val = c->value[i] ? c->value[i] : "";
        int y = 152 + i * 18, room = HM_W - 24 - CAL_RX - pw_text_width(ui->body, val) - 8;

        g->text(g, ui->body, CAL_RX, y, c->label[i] ? c->label[i] : "", room > 0 ? room : 1, p->sub,
                a);
        hm_text_right(g, ui->body, HM_W - 24, y, val, p->text, a);
    }
    if (c->what)
        hm_text_wrap(g, ui->body, 24, 225, 15, 2, c->what, HM_W - 48, 0, p->text, a * 220 / 256);
    hm_hints_right(g, ui, v, HM_W - 16, 264, p->sub, a);
}

void hm_draw_cal(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    for (int d = 1; v->cal && d < HM_DEPTH; d++) /* as it slides in and out */
        if (v->page[d].layout == HM_PAGE_CAL && d <= v->depth + 1 && hm_near(hm_page_t(v, d)) > 0)
            cal_draw(g, ui, v, hm_near(hm_page_t(v, d)));
}

void hm_draw(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    int a = hm_clampi(v->appear, 0, 256);

    g->backdrop(g, v->bg, v->bg_prev && v->bg_mix < 256 ? v->bg_prev : 0, v->bg_mix);
    if (v->aurora > 0 && v->viewer < 256)
        aurora(g, ui, v);
    if (v->viewer >= 256) /* the picture whole; the styles draw only what is over it */
        viewer(g, ui, v);
    if (v->style == HM_STYLE_CLASSIC)
        hm_draw_classic(g, ui, v);
    else if (v->style == HM_STYLE_MINIMAL)
        hm_draw_minimal(g, ui, v);
    else
        hm_draw_modern(g, ui, v);
    osd(g, ui, v);
    if (v->perf_fps)
        perf_overlay(g, ui, v);
    if (a < 256) /* fading in, or out before a game starts */
        g->fill(g, 0, 0, HM_W, HM_H, C_BLACK, 256 - a);
}

void hm_prefetch(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    if (v->style == HM_STYLE_CLASSIC)
        hm_prefetch_classic(g, ui, v);
    else if (v->style == HM_STYLE_MINIMAL)
        hm_prefetch_minimal(g, ui, v);
    else
        hm_prefetch_modern(g, ui, v);
}

/* ---- backgrounds (made once, into 512-stride buffers) ---- */

void hm_make_background(const unsigned int *pic, int w, int h, unsigned int *out)
{
    pw_canvas c = {out, HM_W, HM_H, 512};

    pw_draw_fill(&c, 0, 0, HM_W, HM_H, C_BLACK);
    pw_draw_image_scaled(&c, pic, w, h, 0, 0, HM_W, HM_H, 256);
    pw_draw_blur(&c, 4);
    pw_draw_darken(&c, 150);
    for (int y = 100; y < HM_H; y++) /* darker towards the text */
        pw_draw_blend(&c, 0, y, HM_W, 1, C_BLACK, (y - 100) * 150 / (HM_H - 100));
}

void hm_make_pic1(const unsigned int *pic, int w, int h, unsigned int *out)
{
    pw_canvas c = {out, HM_W, HM_H, 512};

    pw_draw_fill(&c, 0, 0, HM_W, HM_H, C_BLACK);
    pw_draw_image_scaled(&c, pic, w, h, 0, 0, HM_W, HM_H, 256);
    pw_draw_darken(&c, 214); /* the white text and the icons stay readable on bright art */
}

void hm_make_photo(const unsigned int *pic, int w, int h, unsigned int *out)
{
    pw_canvas c = {out, HM_W, HM_H, 512};
    int fw = HM_W, fh = h * HM_W / (w > 0 ? w : 1);

    if (fh > HM_H) {
        fh = HM_H;
        fw = w * HM_H / (h > 0 ? h : 1);
    }
    pw_draw_fill(&c, 0, 0, HM_W, HM_H, C_BLACK);
    pw_draw_image_scaled(&c, pic, w, h, (HM_W - fw) / 2, (HM_H - fh) / 2, fw, fh, 256);
}

void hm_make_gradient(unsigned int accent, unsigned int *out)
{
    pw_canvas c = {out, HM_W, HM_H, 512};

    /* a slate gradient with a faint glow of the accent at the top */
    for (int y = 0; y < HM_H; y++) {
        int t = y * 256 / HM_H;

        pw_draw_fill(&c, 0, y, HM_W, 1,
                     PW_RGB(hm_lerp(26, 8, t), hm_lerp(29, 9, t), hm_lerp(42, 14, t)));
    }
    pw_shape_disc(&c, PW_FX(360), PW_FX(-40), PW_FX(150), accent ? accent : C_ON, 30);
    pw_shape_disc(&c, PW_FX(60), PW_FX(320), PW_FX(120), accent ? accent : C_ON, 14);
}

void hm_make_plain(unsigned int accent, int black, unsigned int *out)
{
    pw_canvas c = {out, HM_W, HM_H, 512};

    if (black) {
        pw_draw_fill(&c, 0, 0, HM_W, HM_H, C_BLACK);
        return;
    }
    /* light: paper white, a breath of the accent at the top */
    for (int y = 0; y < HM_H; y++) {
        int t = y * 256 / HM_H;

        pw_draw_fill(&c, 0, y, HM_W, 1,
                     PW_RGB(hm_lerp(232, 214, t), hm_lerp(235, 218, t), hm_lerp(242, 228, t)));
    }
    pw_shape_disc(&c, PW_FX(380), PW_FX(-60), PW_FX(170), accent ? accent : C_ON, 40);
}

/* A starry night sky (owner 2026-10-23): a near-black sky with a scatter of stars, a soft accent
   glow low down, and a few wave ribbons across the lower half. Computed once (the cheapest
   background: no per-frame work). */
void hm_make_nebula(unsigned int accent, unsigned int *out)
{
    pw_canvas c = {out, HM_W, HM_H, 512};
    unsigned int acc = accent ? accent : C_ON, r = 0x9E3779B9u;

    for (int y = 0; y < HM_H; y++) { /* the night: near-black above, a hint of deep blue below */
        int t = y * 256 / HM_H;

        pw_draw_fill(&c, 0, y, HM_W, 1,
                     PW_RGB(hm_lerp(6, 8, t), hm_lerp(9, 13, t), hm_lerp(16, 28, t)));
    }
    pw_shape_disc(&c, PW_FX(150), PW_FX(255), PW_FX(230), acc, 9); /* soft lights under the waves */
    pw_shape_disc(&c, PW_FX(410), PW_FX(300), PW_FX(180), acc, 6);
    for (int i = 0; i < 150; i++) { /* stars: a deterministic scatter, fewer low down */
        int x, y, b;

        r = r * 1664525u + 1013904223u;
        x = (int)((r >> 9) % HM_W);
        r = r * 1664525u + 1013904223u;
        y = (int)((r >> 9) % HM_H);
        if (y > HM_H / 2 && ((r >> 5) & 1))
            continue;
        r = r * 1664525u + 1013904223u;
        b = 90 + (int)((r >> 9) % 150);
        pw_draw_blend(&c, x, y, 1, 1, PW_RGB(255, 255, 255), b);
        if ((r >> 7) % 9 == 0) { /* a few brighter two-pixel stars */
            pw_draw_blend(&c, x + 1, y, 1, 1, PW_RGB(255, 255, 255), b / 2);
            pw_draw_blend(&c, x, y + 1, 1, 1, PW_RGB(255, 255, 255), b / 2);
        }
    }
    for (int wv = 0; wv < 4; wv++) { /* wave ribbons low down: a gentle crest, a soft glow under */
        int base = 190 + wv * 13, amp = 16 - wv * 3, period = 340 + wv * 60, phase = wv * 60,
            crest = 110 - wv * 20;

        for (int x = 0; x < HM_W; x++) {
            int y = base + amp * isin((unsigned int)(x * 256 / period + phase)) / 256;

            pw_draw_blend(&c, x, y, 1, 1, PW_RGB(228, 236, 255), crest);
            pw_draw_blend(&c, x, y + 1, 1, 2, acc, crest / 3);
            pw_draw_blend(&c, x, y + 3, 1, 5, acc, crest / 10);
        }
    }
}

void hm_make_sky(const pw_image *sky, unsigned int *out)
{
    pw_canvas c = {out, HM_W, HM_H, 512};

    pw_xmbbg_sky(&c, sky);
}

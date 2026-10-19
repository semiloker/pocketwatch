// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "ui/shapes.h"
#include "ui/xmbbg.h"
#include "view_internal.h"

/* ---- what the styles share ---- */

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

void hm_chevron(hm_gfx *g, int x, int y, int dir, unsigned int color, int alpha)
{
    for (int k = 0; k < 4; k++) {
        int dx = dir > 0 ? k - 2 : 2 - k;

        g->fill(g, x + dx, y - 4 + k, 2, 1, color, alpha);
        g->fill(g, x + dx, y + 4 - k, 2, 1, color, alpha);
    }
    g->fill(g, x + (dir > 0 ? 2 : -2), y, 2, 1, color, alpha);
}

void hm_switch(hm_gfx *g, int x, int y, int on, int alpha)
{
    g->rrect(g, x, y - 8, 30, 16, 8, 0, on ? C_ON : C_KNOB, alpha);
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

void hm_art_draw(hm_gfx *g, const pw_image *img, int cx, int cy, int alpha)
{
    g->image(g, img->px, img->w, img->h, (img->w + 15) & ~15, cx - img->w / 2, cy - img->h / 2,
             img->w, img->h, alpha);
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

    if (v->hint_tabs) {
        x -= pw_text_width(ui->body, v->hint_tabs);
        g->text(g, ui->body, x, y, v->hint_tabs, 0, color, alpha);
        x -= pw_text_width(ui->body, "L  R  ") + 4;
        g->text(g, ui->body, x, y, "L  R", 0, C_TEXT, alpha);
        x -= 18;
    }
    if (v->hint_enter) {
        x -= pw_text_width(ui->body, v->hint_enter);
        g->text(g, ui->body, x, y, v->hint_enter, 0, color, alpha);
        g->button(g, ui->cross_enter ? PW_BTN_CROSS : PW_BTN_CIRCLE, x - 9, y - 4, 12, color,
                  alpha);
        x -= 26;
    }
    if (v->hint_options) {
        x -= pw_text_width(ui->body, v->hint_options);
        g->text(g, ui->body, x, y, v->hint_options, 0, color, alpha);
        g->button(g, PW_BTN_TRIANGLE, x - 9, y - 4, 12, color, alpha);
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

/* ---- a photo whole (any style): the picture alone, its name and the buttons for a moment ---- */

static void viewer(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    if (v->viewer_caption && v->caption > 0) {
        int w = pw_text_width(ui->body, v->viewer_caption) + 28;

        w = w > HM_W / 2 - 12 ? HM_W / 2 - 12 : w;
        g->rrect(g, 12, HM_H - 36, w, 24, 12, 0, C_CARD, v->caption * 200 / 256);
        g->text(g, ui->body, 26, HM_H - 20, v->viewer_caption, w - 28, C_TEXT, v->caption);
    }
    if (v->caption > 0 && (v->viewer_wallpaper || v->viewer_slideshow)) {
        /* what the buttons do, bottom right, on a pill of their own */
        int ww = v->viewer_wallpaper ? pw_text_width(ui->body, v->viewer_wallpaper) : 0,
            sw = v->viewer_slideshow ? pw_text_width(ui->body, v->viewer_slideshow) : 0,
            w = 22 + (sw ? 15 + sw : 0) + (ww ? 15 + ww : 0) + (sw && ww ? 12 : 0),
            x = HM_W - 12 - w + 10; /* a group: the glyph (centred 6 in), its text 15 in */

        g->rrect(g, HM_W - 12 - w, HM_H - 36, w, 24, 12, 0, C_CARD, v->caption * 200 / 256);
        if (sw) {
            g->button(g, PW_BTN_TRIANGLE, x + 6, HM_H - 24, 12, C_TEXT, v->caption);
            g->text(g, ui->body, x + 15, HM_H - 20, v->viewer_slideshow, 0, C_TEXT, v->caption);
            x += 15 + sw + 12;
        }
        if (ww) {
            g->button(g, PW_BTN_SQUARE, x + 6, HM_H - 24, 12, C_TEXT, v->caption);
            g->text(g, ui->body, x + 15, HM_H - 20, v->viewer_wallpaper, 0, C_TEXT, v->caption);
        }
    }
}

void hm_draw(hm_gfx *g, const hm_ui *ui, const hm_view *v)
{
    int a = hm_clampi(v->appear, 0, 256);

    g->backdrop(g, v->bg, v->bg_prev && v->bg_mix < 256 ? v->bg_prev : 0, v->bg_mix);
    if (v->viewer >= 256) {
        viewer(g, ui, v);
        return;
    }
    if (v->style == HM_STYLE_CLASSIC)
        hm_draw_classic(g, ui, v);
    else if (v->style == HM_STYLE_MINIMAL)
        hm_draw_minimal(g, ui, v);
    else
        hm_draw_modern(g, ui, v);
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

void hm_make_gradient(unsigned int *out)
{
    pw_canvas c = {out, HM_W, HM_H, 512};

    /* a slate gradient with a faint glow of the accent at the top */
    for (int y = 0; y < HM_H; y++) {
        int t = y * 256 / HM_H;

        pw_draw_fill(&c, 0, y, HM_W, 1,
                     PW_RGB(hm_lerp(26, 8, t), hm_lerp(29, 9, t), hm_lerp(42, 14, t)));
    }
    pw_shape_disc(&c, PW_FX(360), PW_FX(-40), PW_FX(150), C_ON, 30);
}

void hm_make_sky(const pw_image *sky, unsigned int *out)
{
    pw_canvas c = {out, HM_W, HM_H, 512};

    pw_xmbbg_sky(&c, sky);
}

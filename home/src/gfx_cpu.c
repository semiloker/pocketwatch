// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "gfx_cpu.h"
#include "ui/icons.h"
#include "ui/shapes.h"

static pw_canvas *canvas(hm_gfx *g)
{
    return ((hm_gfx_cpu *)g)->c;
}

static void fill(hm_gfx *g, int x, int y, int w, int h, unsigned int color, int alpha)
{
    pw_draw_blend(canvas(g), x, y, w, h, color, alpha);
}

static void rrect(hm_gfx *g, int x, int y, int w, int h, int r, int line, unsigned int color,
                  int alpha)
{
    pw_shape_rrect(canvas(g), PW_FX(x), PW_FX(y), PW_FX(w), PW_FX(h), PW_FX(r), line, color, alpha);
}

static void image(hm_gfx *g, const unsigned int *px, int w, int h, int stride, int x, int y, int dw,
                  int dh, int alpha)
{
    (void)stride; /* the CPU path takes packed images: every caller's stride is its width */
    pw_draw_image_scaled(canvas(g), px, w, h, x, y, dw, dh, alpha);
}

static void backdrop(hm_gfx *g, const unsigned int *bg, const unsigned int *prev, int mix)
{
    pw_canvas *c = canvas(g);

    if (bg && prev && mix < 256) {
        pw_draw_lerp(c, prev, bg, mix);
    } else if (bg) {
        for (int y = 0; y < c->h; y++)
            memcpy(c->px + y * c->stride, bg + y * 512, (size_t)c->w * 4);
    } else {
        pw_draw_fill(c, 0, 0, c->w, c->h, PW_RGB(0, 0, 0));
    }
}

static void text(hm_gfx *g, pw_font *f, int x, int y, const char *s, int max_w, unsigned int color,
                 int alpha)
{
    if (max_w > 0)
        pw_text_draw_fit(canvas(g), f, x, y, s, max_w, color, alpha, 0);
    else
        pw_text_draw(canvas(g), f, x, y, s, color, alpha, 0);
}

static void icon(hm_gfx *g, int which, int cx, int cy, int size, unsigned int color, int alpha)
{
    pw_icon_draw(canvas(g), which, cx, cy, size, color, alpha);
}

static void button(hm_gfx *g, int which, int cx, int cy, int size, unsigned int color, int alpha)
{
    pw_button_draw(canvas(g), which, cx, cy, size, color, alpha);
}

static int warm(hm_gfx *g, pw_font *f, const char *s, int max_w)
{
    (void)g;
    (void)f;
    (void)s;
    (void)max_w;
    return 1; /* nothing to prepare: the CPU draws text as it goes */
}

void hm_gfx_cpu_init(hm_gfx_cpu *g, pw_canvas *c)
{
    g->ops.fill = fill;
    g->ops.rrect = rrect;
    g->ops.image = image;
    g->ops.backdrop = backdrop;
    g->ops.text = text;
    g->ops.icon = icon;
    g->ops.button = button;
    g->ops.warm = warm;
    g->c = c;
}

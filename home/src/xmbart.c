// SPDX-License-Identifier: GPL-3.0-or-later
#include <malloc.h>
#include <stdlib.h>
#include <string.h>

#include "gfx/rco.h"
#include "gfx/rlz.h"
#include "xmbart.h"

#define TABLES_MAX (64 * 1024)
#define GIM_MAX    (64 * 1024)
#define PIXELS_MAX (128 * 128)

/* Labels as the game plugin uses them (gfx/assets.c; categories VERIFIED in
   docs/research/m4-native-icons.md: AJ Settings, AL Photo, AM Music, AO Game). Play Time has
   no XMB category: Home draws its watch. */
static const char *const k_cat[HM_TABS] = {"AO", "AM", "AL", 0, "AJ", "BX"};
static const char *const k_btn[4] = {"tex_cross", "tex_circle", "tex_triangle", "tex_square"};

/* A decoded picture into aligned memory with rows of a multiple of 16 pixels. */
static int keep(pw_image *out, const pw_image *img)
{
    int stride = (img->w + 15) & ~15;
    unsigned int *px = memalign(16, (size_t)stride * (size_t)img->h * 4);

    if (!px)
        return -1;
    memset(px, 0, (size_t)stride * (size_t)img->h * 4);
    for (int y = 0; y < img->h; y++)
        memcpy(px + y * stride, img->px + y * img->w, (size_t)img->w * 4);
    out->px = px;
    out->w = img->w;
    out->h = img->h;
    return 0;
}

typedef struct {
    pw_rco r;
    unsigned char *file, *tables, *gim, *state;
    unsigned int *pixels;
} rco_ctx;

static int one(rco_ctx *c, const char *label, pw_image *out)
{
    pw_rco_image im;
    pw_image img;
    int got;

    if (!label || pw_rco_find_image(&c->r, label, &im) < 0 ||
        (got = pw_rco_read_image(&c->r, &im, c->gim, GIM_MAX, c->state, 0)) < 0 ||
        pw_gim_decode(c->gim, (unsigned int)got, c->pixels, PIXELS_MAX, &img) < 0)
        return 0;
    return keep(out, &img) == 0;
}

static int open_rco(rco_ctx *c, hm_art_read read, const char *path)
{
    unsigned int len = 0;

    c->file = read(path, &len);
    return c->file && pw_rco_open(&c->r, c->file, len, c->tables, TABLES_MAX, c->state, 0) == 0;
}

int hm_art_load(hm_art *a, hm_art_read read, const char *icons_rco, const char *fg_rco)
{
    rco_ctx c;
    int found = 0;

    memset(a, 0, sizeof(*a));
    memset(&c, 0, sizeof(c));
    c.tables = malloc(TABLES_MAX);
    c.gim = malloc(GIM_MAX);
    c.state = malloc(PW_RLZ_STATE);
    c.pixels = malloc(PIXELS_MAX * 4);
    if (c.tables && c.gim && c.state && c.pixels) {
        if (open_rco(&c, read, icons_rco))
            for (int i = 0; i < HM_TABS; i++)
                found += one(&c, k_cat[i], &a->cat[i]);
        free(c.file);
        c.file = 0;
        if (open_rco(&c, read, fg_rco)) {
            for (int i = 0; i < 4; i++)
                found += one(&c, k_btn[i], &a->btn[i]);
            found += one(&c, "tex_battery", &a->battery);
        }
        free(c.file);
    }
    free(c.tables);
    free(c.gim);
    free(c.state);
    free(c.pixels);
    return found;
}

int hm_art_sky(pw_image *sky, hm_art_read read, const char *sky_bmp, int index)
{
    unsigned int len = 0;
    unsigned char *file = read(sky_bmp, &len);
    unsigned int *px = malloc(64 * 64 * 4);
    int r = !file ? -1 : !px ? -2 : pw_bmp_decode(file, len, index, px, 64 * 64, sky);

    if (r < 0 && file && px && pw_bmp_decode(file, len, 0, px, 64 * 64, sky) == 0)
        r = 0;
    else if (r < 0 && file && px)
        r -= 10; /* the decoder's answer: -11..-13 */
    free(file);
    if (r < 0) {
        free(px);
        sky->px = 0;
    }
    return r;
}

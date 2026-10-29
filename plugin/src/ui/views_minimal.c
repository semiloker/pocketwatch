// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * The minimal style (docs/ui/styles.md): the game darkened, nothing but text in one centred column,
 * the chosen line bright and the others faint, a thin underline for choices. No icons, no boxes.
 */
#include "ui/i18n.h"
#include "ui/icons.h"
#include "ui/shapes.h"
#include "ui/views_style.h"

#define SCREEN_W 480
#define SCREEN_H 272

#define C_TEXT PW_RGB(255, 255, 255)
#define C_DIM  PW_RGB(150, 150, 150)
#define FAINT  95 /* alpha of the lines that are not chosen */

#define LIST_CY    136
#define LINE_PITCH 28

static int clampi(int v, int lo, int hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

static void text_center(pw_canvas *c, pw_font *f, int cx, int y, const char *s, int max,
                        unsigned int color, int alpha)
{
    int w = pw_text_width(f, s);

    if (w > max)
        w = max;
    pw_text_draw_fit(c, f, cx - w / 2, y, s, max, color, alpha, 0);
}

void pw_minimal_hints(pw_canvas *c, const pw_ui *ui, const char *enter_label,
                      const char *back_label, int alpha)
{
    const char *cross = ui->cross_enter ? enter_label : back_label;
    const char *circle = ui->cross_enter ? back_label : enter_label;
    int wc = cross ? 14 + pw_text_width(ui->body, cross) : 0;
    int wo = circle ? 14 + pw_text_width(ui->body, circle) : 0;
    int x = SCREEN_W / 2 - (wc + wo + (cross && circle ? 20 : 0)) / 2;

    alpha = alpha * 180 / 256; /* quieter than the content */
    if (cross) {
        pw_view_button(c, ui, PW_BTN_CROSS, x + 5, 254, alpha);
        pw_text_draw(c, ui->body, x + 14, 258, cross, C_DIM, alpha, 0);
        x += wc + 20;
    }
    if (circle) {
        pw_view_button(c, ui, PW_BTN_CIRCLE, x + 5, 254, alpha);
        pw_text_draw(c, ui->body, x + 14, 258, circle, C_DIM, alpha, 0);
    }
}

void pw_minimal_status(pw_canvas *c, const pw_ui *ui, const char *clock, int battery, int charging,
                       int alpha, unsigned int t_ms)
{
    (void)t_ms;
    char line[32];
    int n = 0;

    (void)charging;
    for (const char *p = clock; p && *p && n < 20; p++)
        line[n++] = *p;
    if (battery >= 0 && n < 22) {
        int v = clampi(battery, 0, 100);

        line[n++] = ' ';
        line[n++] = ' ';
        if (v >= 100)
            line[n++] = '1';
        if (v >= 10)
            line[n++] = (char)('0' + v / 10 % 10);
        line[n++] = (char)('0' + v % 10);
        line[n++] = '%';
    }
    line[n] = 0;
    if (n)
        pw_text_draw(c, ui->body, SCREEN_W - 20 - pw_text_width(ui->body, line), 24, line, C_DIM,
                     alpha, 0);
}

/* One centred column: the chosen line at the middle, the others faint above and below. */
static void column(pw_canvas *c, pw_font *f, const char *const *labels, const int *enabled,
                   int count, int selpos, int fade)
{
    for (int i = 0; i < count; i++) {
        int d = i * 256 - selpos, y = LIST_CY + d * LINE_PITCH / 256;
        int near = 256 - clampi(d < 0 ? -d : d, 0, 256);
        int alpha = FAINT + (256 - FAINT) * near / 256;

        if (y < 40 || y > SCREEN_H - 30)
            continue;
        if (enabled && !enabled[i])
            alpha = alpha / 2;
        text_center(c, f, SCREEN_W / 2, y, labels[i], 400, C_TEXT, alpha * fade / 256);
    }
}

void pw_minimal_hub(pw_canvas *c, const pw_ui *ui, const pw_hub_cat *cat, const pw_hub_row *rows,
                    int count, int selpos, int fade)
{
    const char *labels[16];
    int enabled[16], sel = clampi((selpos + 128) / 256, 0, count - 1);

    if (count > 16)
        count = 16;
    for (int i = 0; i < count; i++) {
        labels[i] = rows[i].title;
        enabled[i] = rows[i].enabled;
    }
    if (cat->subtitle)
        pw_text_draw_fit(c, ui->body, 20, 24, cat->subtitle, 250, C_DIM, fade, 0);
    column(c, ui->title, labels, enabled, count, selpos, fade);
    /* a short rule under the chosen line, and its note further down */
    pw_draw_blend(c, SCREEN_W / 2 - 12, LIST_CY + 7, 24, 1, C_TEXT, fade * 200 / 256);
    if (count > 0 && rows[sel].desc)
        text_center(c, ui->body, SCREEN_W / 2, 226, rows[sel].desc, 440, C_DIM, fade);
    pw_minimal_hints(c, ui, pw_tr(PW_S_ENTER), pw_tr(PW_S_BACK), fade);
}

static void choice(pw_canvas *c, const pw_ui *ui, int cx, int y, const char *label, int selected,
                   int alpha)
{
    int w = pw_text_width(ui->title, label);

    pw_text_draw(c, ui->title, cx - w / 2, y, label, C_TEXT, (selected ? 256 : FAINT) * alpha / 256,
                 0);
    if (selected)
        pw_draw_blend(c, cx - w / 2, y + 5, w, 1, C_TEXT, 230 * alpha / 256);
}

void pw_minimal_dialog(pw_canvas *c, const pw_ui *ui, int kind, const char *line1,
                       const char *line2, int yes, int appear)
{
    char lines[4][PW_VIEW_LINE_MAX];
    int n = pw_view_wrap(ui->title, line1, 380, lines, 0, line2 ? 2 : 3), y0;
    int a = clampi(appear, 0, 256);

    if (line2)
        n = pw_view_wrap(ui->title, line2, 380, lines, n, n + 2 > 4 ? 4 : n + 2);
    pw_draw_blend(c, 0, 0, SCREEN_W, SCREEN_H, PW_RGB(0, 0, 0), 130 * a / 256);
    y0 = 128 - (n - 1) * 10;
    for (int i = 0; i < n; i++)
        text_center(c, ui->title, SCREEN_W / 2, y0 + i * 20, lines[i], 420, C_TEXT,
                    (i == 0 ? 256 : 170) * a / 256);
    if (kind == PW_VIEW_CONFIRM) {
        choice(c, ui, SCREEN_W / 2 - 44, y0 + n * 20 + 22, pw_tr(PW_S_YES), yes, a);
        choice(c, ui, SCREEN_W / 2 + 44, y0 + n * 20 + 22, pw_tr(PW_S_NO), !yes, a);
        pw_minimal_hints(c, ui, pw_tr(PW_S_ENTER), pw_tr(PW_S_BACK), a);
    } else {
        choice(c, ui, SCREEN_W / 2, y0 + n * 20 + 22, pw_tr(PW_S_OK), 1, a);
        pw_minimal_hints(c, ui, pw_tr(PW_S_ENTER), 0, a);
    }
}

void pw_minimal_busy(pw_canvas *c, const pw_ui *ui, const char *text, unsigned int t_ms)
{
    int phase = (int)(t_ms / 300) % 4, w = pw_text_width(ui->title, text);

    text_center(c, ui->title, SCREEN_W / 2, LIST_CY, text, 440, C_TEXT, 256);
    for (int i = 0; i < 3; i++) /* three dots that fill in turn */
        pw_shape_disc(c, PW_FX(SCREEN_W / 2 - 8 + i * 8), PW_FX(LIST_CY + 18), PW_FX(1) + 8, C_TEXT,
                      i < phase ? 230 : 60);
    (void)w;
}

/* The state list: the slots as lines on the left, the chosen one's picture on the right. */
#define SLOT_X   36
#define SLOT_PIT 40
#define PIC_X    250
#define PIC_Y    66
#define PIC_W    192
#define PIC_H    108

void pw_minimal_slots(pw_canvas *c, const pw_ui *ui, const char *heading, const char *game,
                      const pw_slot_row *rows, int count, int selpos, int can_delete, int appear)
{
    int sel = clampi((selpos + 128) / 256, 0, count - 1), off = sel * 256 - selpos;
    int dn = 256 - 2 * (off < 0 ? -off : off), pic_a = (dn < 0 ? 0 : dn) * appear / 256;
    const pw_slot_row *r = &rows[sel];

    pw_text_draw_fit(c, ui->body, 20, 24, heading, 200, C_DIM, appear, 0);
    if (game)
        pw_text_draw_fit(c, ui->body, PIC_X, PIC_Y + PIC_H + 40, game, SCREEN_W - PIC_X - 10, C_DIM,
                         appear, 0);
    for (int i = 0; i < count; i++) { /* the lines glide past a fixed mark */
        int p = i * 256 - selpos, y = LIST_CY + p * SLOT_PIT / 256;
        int near = 256 - clampi(p < 0 ? -p : p, 0, 256);

        if (y < 44 || y > SCREEN_H - 34)
            continue;
        pw_text_draw_fit(c, ui->title, SLOT_X, y, rows[i].title, PIC_X - SLOT_X - 16, C_TEXT,
                         (FAINT + (256 - FAINT) * near / 256) * appear / 256, 0);
    }
    pw_draw_blend(c, SLOT_X - 14, LIST_CY - 9, 2, 12, C_TEXT, 230 * appear / 256);
    if (r->exists && r->icon) {
        pw_draw_image_scaled(c, r->icon, 144, 80, PIC_X, PIC_Y, PIC_W, PIC_H, pic_a);
    } else {
        int cx = PIC_X + PIC_W / 2, cy = PIC_Y + PIC_H / 2;

        pw_shape_rrect(c, PW_FX(PIC_X), PW_FX(PIC_Y), PW_FX(PIC_W), PW_FX(PIC_H), PW_FX(2),
                       PW_FX(1), C_DIM, 160 * pic_a / 256);
        pw_shape_capsule(c, PW_FX(cx - 9), PW_FX(cy), PW_FX(cx + 9), PW_FX(cy), PW_FX(1), C_DIM,
                         200 * pic_a / 256);
        pw_shape_capsule(c, PW_FX(cx), PW_FX(cy - 9), PW_FX(cx), PW_FX(cy + 9), PW_FX(1), C_DIM,
                         200 * pic_a / 256);
    }
    if (r->info)
        pw_text_draw_fit(c, ui->body, PIC_X, PIC_Y + PIC_H + 20, r->info, SCREEN_W - PIC_X - 10,
                         C_TEXT, 220 * pic_a / 256, 0);
    pw_minimal_hints(c, ui, pw_tr(PW_S_ENTER), pw_tr(PW_S_BACK), 256);
    if (can_delete && r->exists) {
        pw_view_button(c, ui, PW_BTN_TRIANGLE, SCREEN_W - 70, 254, 180);
        pw_text_draw(c, ui->body, SCREEN_W - 61, 258, pw_tr(PW_S_DELETE), C_DIM, 180, 0);
    }
}

/* A thin line at the bottom and the number, nothing else. */
void pw_minimal_volume(pw_canvas *c, const pw_ui *ui, const pw_volume *v)
{
    int a = v->alpha, w = 160, x = (SCREEN_W - w) / 2 - 14, y = SCREEN_H - 40, /* over the hints */
        fill = w * v->shown / (PW_VOLUME_MAX * 256);
    char num[4];

    if (a <= 0)
        return;
    pw_draw_blend(c, x, y, w, 2, C_DIM, a * 90 / 256);
    pw_draw_blend(c, x, y, fill, 2, v->muted ? C_DIM : C_TEXT, a);
    if (v->muted) {
        num[0] = num[1] = '-';
        num[2] = 0;
    } else {
        num[0] = (char)(v->level >= 10 ? '0' + v->level / 10 : '0' + v->level);
        num[1] = (char)(v->level >= 10 ? '0' + v->level % 10 : 0);
        num[2] = 0;
    }
    pw_text_draw(c, ui->body, x + w + 10, y + 6, num, v->flash ? C_TEXT : C_DIM, a, 0);
}

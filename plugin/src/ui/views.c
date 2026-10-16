// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/views.h"
#include "ui/icons.h"
#include "ui/shapes.h"

#define SCREEN_W 480
#define SCREEN_H 272

/*
 * Hub: the XMB's column, measured on real 480x272 XMB captures (ARK-4's screenshots, 2026-10-16):
 * open category icon centred at (109, 70) with its label below, selected item's icon centred at
 * (104, 135) and its text from x 150, the next items 65 px apart below, the previous one above
 * the category row; unselected items at about half opacity, same size.
 */
#define CAT_X       109
#define CAT_Y       70
#define CAT_LABEL_Y 101
#define ICON_X      104
#define TEXT_X      150
#define SEL_Y       135
#define ROW_PITCH   65
#define ABOVE_Y     20 /* the row just before the cursor */
#define DIM_ALPHA   120
#define TEXT_MAX    (SCREEN_W - TEXT_X - 6)

/* Dialog: the in-game HOME screen ("Do you want to quit the game?"), PPSSPP's metrics. */
#define RULE_X0  60
#define RULE_X1  420
#define RULE_TOP 98
#define RULE_BOT 177
#define TEXT_Y1  126
#define TEXT_Y2  144
#define CHOICE_Y 165
#define YES_X    204
#define NO_X     273

void pw_view_text_centered(pw_canvas *c, pw_font *f, int cx, int y, const char *s,
                           unsigned int color, int alpha)
{
    int w = pw_text_width(f, s), max = RULE_X1 - RULE_X0 + 20;

    if (w > max)
        w = max;
    pw_text_draw_fit(c, f, cx - w / 2, y, s, max, color, alpha, 1);
}

static void image_centered(pw_canvas *c, const pw_image *img, int cx, int cy, int alpha)
{
    pw_draw_image(c, img->px, img->w, img->h, cx - img->w / 2, cy - img->h / 2, alpha);
}

static void button(pw_canvas *c, const pw_ui *ui, int which, int cx, int cy, int alpha)
{
    const pw_image *img = ui->btn[which];

    if (img)
        image_centered(c, img, cx, cy, alpha);
    else
        pw_button_draw(c, which, cx, cy, 12, PW_C_TEXT, alpha);
}

void pw_view_hints(pw_canvas *c, const pw_ui *ui, const char *enter_label, const char *back_label,
                   int alpha)
{
    /* The PSP always shows X left of O and swaps only what they mean. */
    const char *cross = ui->cross_enter ? enter_label : back_label;
    const char *circle = ui->cross_enter ? back_label : enter_label;
    int wc = cross ? 14 + pw_text_width(ui->body, cross) : 0;
    int wo = circle ? 14 + pw_text_width(ui->body, circle) : 0;
    int x = SCREEN_W / 2 - (wc + wo + (cross && circle ? 16 : 0)) / 2;

    if (cross) {
        button(c, ui, PW_BTN_CROSS, x + 6, 256, alpha);
        pw_text_draw(c, ui->body, x + 14, 260, cross, PW_C_TEXT, alpha, 1);
        x += wc + 16;
    }
    if (circle) {
        button(c, ui, PW_BTN_CIRCLE, x + 6, 256, alpha);
        pw_text_draw(c, ui->body, x + 14, 260, circle, PW_C_TEXT, alpha, 1);
    }
}

void pw_view_status(pw_canvas *c, const pw_ui *ui, const char *clock, int battery, int charging,
                    int alpha)
{
    /* the battery: an outlined cell with three bars, its tip on the right (XMB at 451..474) */
    if (battery >= 0) {
        int bars = battery > 66 ? 3 : battery > 33 ? 2 : battery > 5 ? 1 : 0;

        pw_shape_rrect(c, PW_FX(451), PW_FX(7), PW_FX(21), PW_FX(10), PW_FX(2), PW_FX(1) + 4,
                       PW_C_TEXT, alpha);
        pw_draw_blend(c, 472, 10, 2, 4, PW_C_TEXT, alpha);
        for (int i = 0; i < bars; i++)
            pw_draw_blend(c, 453 + i * 6, 9, 5, 6, PW_C_TEXT, alpha);
        if (charging) /* a lightning mark over the cell */
            pw_shape_triangle(c, PW_FX(463), PW_FX(8), PW_FX(458), PW_FX(13), PW_FX(462), PW_FX(13),
                              PW_RGB(255, 220, 0), alpha);
    }
    if (clock && clock[0])
        pw_text_draw(c, ui->body, 444 - pw_text_width(ui->body, clock), 17, clock, PW_C_TEXT, alpha,
                     1);
}

/* Row centre for a position relative to the cursor (1/256 rows). */
static int row_y(int p)
{
    if (p >= 0)
        return SEL_Y + ROW_PITCH * p / 256;
    if (p >= -256)
        return SEL_Y + (SEL_Y - ABOVE_Y) * p / 256;
    return ABOVE_Y + ROW_PITCH * (p + 256) / 256;
}

static void row_icon(pw_canvas *c, const pw_hub_row *r, int y, int near, int alpha)
{
    if (r->icon) {
        if (r->glow && near > 0) { /* cross-fade to the glowing picture, like the XMB */
            image_centered(c, r->icon, ICON_X, y, alpha * (256 - near) / 256);
            image_centered(c, r->glow, ICON_X, y, alpha * near / 256);
        } else {
            image_centered(c, r->icon, ICON_X, y, alpha);
        }
    } else {
        pw_icon_draw(c, r->fallback, ICON_X, y, 40 + 8 * near / 256, PW_C_TEXT, alpha);
    }
}

void pw_view_hub(pw_canvas *c, const pw_ui *ui, const pw_hub_cat *cat, const pw_hub_row *rows,
                 int count, int selpos, int fade)
{
    if (cat->icon)
        image_centered(c, cat->icon, CAT_X, CAT_Y, fade);
    else
        pw_icon_draw(c, cat->fallback, CAT_X, CAT_Y, 50, PW_C_TEXT, fade);
    if (cat->label)
        pw_text_draw(c, ui->body, CAT_X - pw_text_width(ui->body, cat->label) / 2, CAT_LABEL_Y,
                     cat->label, PW_C_TEXT, fade, 1);

    for (int i = 0; i < count; i++) {
        const pw_hub_row *r = &rows[i];
        int p = i * 256 - selpos, ap = p < 0 ? -p : p, y = row_y(p);
        int near = ap >= 256 ? 0 : 256 - ap; /* 256 on the cursor, 0 one row away */
        int alpha = DIM_ALPHA + (256 - DIM_ALPHA) * near / 256;

        if (y < -40 || y > SCREEN_H + 40)
            continue;
        if (!r->enabled)
            alpha = alpha * 2 / 3;
        alpha = alpha * fade / 256;
        row_icon(c, r, y, near, alpha);
        if (r->desc && near > 128) {
            pw_text_draw_fit(c, ui->title, TEXT_X, y + 2, r->title, TEXT_MAX, PW_C_TEXT, alpha, 1);
            pw_text_draw_fit(c, ui->body, TEXT_X, y + 18, r->desc, TEXT_MAX, PW_C_TEXT,
                             alpha * (near - 128) / 128, 1);
        } else {
            pw_text_draw_fit(c, ui->title, TEXT_X, y + 5, r->title, TEXT_MAX, PW_C_TEXT, alpha, 1);
        }
    }
}

static void choice(pw_canvas *c, const pw_ui *ui, int x, const char *label, int selected)
{
    int w = pw_text_width(ui->title, label), bw = w + 8 > 30 ? w + 8 : 30;

    if (selected) /* the HOME screen's light box behind the chosen answer */
        pw_draw_blend(c, x - bw / 2, CHOICE_Y - 12, bw, 16, PW_RGB(0xCF, 0xCF, 0xCF), 110);
    pw_text_draw(c, ui->title, x - w / 2, CHOICE_Y, label, PW_C_TEXT, 256, 1);
}

void pw_view_dialog(pw_canvas *c, const pw_ui *ui, int kind, const char *line1, const char *line2,
                    int yes)
{
    pw_draw_fill(c, RULE_X0, RULE_TOP, RULE_X1 - RULE_X0, 1, PW_C_TEXT);
    pw_draw_fill(c, RULE_X0, RULE_BOT, RULE_X1 - RULE_X0, 1, PW_C_TEXT);
    if (line2) {
        pw_view_text_centered(c, ui->title, SCREEN_W / 2, TEXT_Y1, line1, PW_C_TEXT, 256);
        pw_view_text_centered(c, ui->title, SCREEN_W / 2, TEXT_Y2, line2, PW_C_TEXT, 256);
    } else {
        pw_view_text_centered(c, ui->title, SCREEN_W / 2, (TEXT_Y1 + TEXT_Y2) / 2, line1, PW_C_TEXT,
                              256);
    }
    if (kind == PW_VIEW_CONFIRM) {
        choice(c, ui, YES_X, "Yes", yes);
        choice(c, ui, NO_X, "No", !yes);
        pw_view_hints(c, ui, "Enter", "Back", 256);
    } else {
        choice(c, ui, SCREEN_W / 2, "OK", 1);
        pw_view_hints(c, ui, "Enter", 0, 256);
    }
}

void pw_view_busy(pw_canvas *c, const pw_ui *ui, const char *text, unsigned int t_ms)
{
    int cx = SCREEN_W / 2, cy = 120, head = (int)(t_ms / 90) % 8;

    for (int i = 0; i < 8; i++) { /* eight dots, the brightest one turning */
        static const signed char k[8][2] = {{0, -12}, {8, -8}, {12, 0},  {8, 8},
                                            {0, 12},  {-8, 8}, {-12, 0}, {-8, -8}};
        int age = (head - i + 8) % 8;

        pw_shape_disc(c, PW_FX(cx + k[i][0]), PW_FX(cy + k[i][1]), PW_FX(2) + 6, PW_C_TEXT,
                      256 - age * 28);
    }
    pw_view_text_centered(c, ui->title, cx, 158, text, PW_C_TEXT, 256);
}

/*
 * State list: the Saved Data Utility (PPSSPP PSPSaveDialog.cpp): a banner, the selected slot's
 * 144x80 icon at (27, 97) with a white frame, the others at 81x45 above and below, the details on
 * the right under a white rule.
 */
#define LIST_ICON_X  27
#define LIST_ICON_Y  97
#define LIST_SMALL_W 81
#define LIST_SMALL_H 45
#define LIST_SMALL_X 58
#define DETAIL_X     180

static void slot_icon(pw_canvas *c, const pw_slot_row *r, int x, int y, int w, int h, int alpha)
{
    pw_draw_blend(c, x, y, w, h, PW_C_BLACK, alpha * 150 / 256);
    if (r->exists && r->icon) {
        pw_draw_image_scaled(c, r->icon, 144, 80, x, y, w, h, alpha);
    } else { /* a new slot: a plus in the box */
        int cx = PW_FX(x + w / 2), cy = PW_FX(y + h / 2), arm = PW_FX(h / 5);

        pw_shape_capsule(c, cx - arm, cy, cx + arm, cy, PW_FX(1), PW_C_TEXT, alpha);
        pw_shape_capsule(c, cx, cy - arm, cx, cy + arm, PW_FX(1), PW_C_TEXT, alpha);
    }
}

void pw_view_slots(pw_canvas *c, const pw_ui *ui, const char *heading, const char *game,
                   const pw_slot_row *rows, int count, int sel, int can_delete)
{
    const pw_slot_row *r = &rows[sel];

    for (int i = 0; i < count; i++) {
        int k = i - sel, y;

        if (k == 0)
            continue;
        y = k < 0 ? LIST_ICON_Y - 6 - LIST_SMALL_H + (k + 1) * (LIST_SMALL_H + 6)
                  : LIST_ICON_Y + 80 + 6 + (k - 1) * (LIST_SMALL_H + 6);
        if (y < 23 - LIST_SMALL_H || y > SCREEN_H)
            continue;
        slot_icon(c, &rows[i], LIST_SMALL_X, y, LIST_SMALL_W, LIST_SMALL_H, 200);
    }
    slot_icon(c, r, LIST_ICON_X, LIST_ICON_Y, 144, 80, 256);
    pw_draw_fill(c, LIST_ICON_X - 1, LIST_ICON_Y - 1, 146, 1, PW_C_TEXT);
    pw_draw_fill(c, LIST_ICON_X - 1, LIST_ICON_Y + 80, 146, 1, PW_C_TEXT);
    pw_draw_fill(c, LIST_ICON_X - 1, LIST_ICON_Y, 1, 80, PW_C_TEXT);
    pw_draw_fill(c, LIST_ICON_X + 144, LIST_ICON_Y, 1, 80, PW_C_TEXT);
    /* the banner over everything that scrolls under it */
    pw_draw_blend(c, 0, 0, SCREEN_W, 23, PW_RGB(0x58, 0x63, 0x63), 160);
    pw_text_draw(c, ui->title, 30, 17, heading, PW_C_TEXT, 256, 1);

    pw_text_draw_fit(c, ui->title, DETAIL_X, 132, game, SCREEN_W - DETAIL_X - 6, PW_C_GREY, 256, 1);
    pw_draw_fill(c, DETAIL_X, 136, SCREEN_W - DETAIL_X, 1, PW_C_TEXT);
    if (r->info)
        pw_text_draw(c, ui->body, DETAIL_X, 152, r->info, PW_C_TEXT, 256, 1);
    pw_text_draw_fit(c, ui->title, 175, 174, r->title, SCREEN_W - 181, PW_C_TEXT, 256, 1);
    if (r->exists)
        pw_text_draw(c, ui->body, 175, 192, "Pocketwatch", PW_C_GREY, 256, 1);

    pw_view_hints(c, ui, "Enter", "Back", 256);
    if (can_delete && r->exists) {
        button(c, ui, PW_BTN_TRIANGLE, 400, 256, 256);
        pw_text_draw(c, ui->body, 408, 260, "Delete", PW_C_TEXT, 256, 1);
    }
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "ui/views.h"
#include "ui/icons.h"
#include "ui/shapes.h"
#include "ui/i18n.h"
#include "ui/views_style.h"

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

void pw_view_button(pw_canvas *c, const pw_ui *ui, int which, int cx, int cy, int alpha)
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
    if (ui->style == PW_STYLE_MODERN) {
        pw_modern_hints(c, ui, enter_label, back_label, alpha);
        return;
    }
    if (ui->style == PW_STYLE_MINIMAL) {
        pw_minimal_hints(c, ui, enter_label, back_label, alpha);
        return;
    }
    /* The PSP always shows X left of O and swaps only what they mean. */
    const char *cross = ui->cross_enter ? enter_label : back_label;
    const char *circle = ui->cross_enter ? back_label : enter_label;
    int wc = cross ? 14 + pw_text_width(ui->body, cross) : 0;
    int wo = circle ? 14 + pw_text_width(ui->body, circle) : 0;
    int x = SCREEN_W / 2 - (wc + wo + (cross && circle ? 16 : 0)) / 2;

    if (cross) {
        pw_view_button(c, ui, PW_BTN_CROSS, x + 6, 256, alpha);
        pw_text_draw(c, ui->body, x + 14, 260, cross, PW_C_TEXT, alpha, 1);
        x += wc + 16;
    }
    if (circle) {
        pw_view_button(c, ui, PW_BTN_CIRCLE, x + 6, 256, alpha);
        pw_text_draw(c, ui->body, x + 14, 260, circle, PW_C_TEXT, alpha, 1);
    }
}

void pw_view_status(pw_canvas *c, const pw_ui *ui, const char *clock, int battery, int charging,
                    int alpha, unsigned int t_ms)
{
    if (ui->style == PW_STYLE_MODERN) {
        pw_modern_status(c, ui, clock, battery, charging, alpha, t_ms);
        return;
    }
    if (ui->style == PW_STYLE_MINIMAL) {
        pw_minimal_status(c, ui, clock, battery, charging, alpha, t_ms);
        return;
    }
    if (battery >= 0 && ui->battery && ui->battery->h >= 4 && ui->battery->h % 4 == 0) {
        /* the XMB's own picture: four frames, full to empty, centred at (463, 12); charging, it
           fills up in a loop */
        const pw_image *b = ui->battery;
        int fh = b->h / 4, frame = battery > 66 ? 0 : battery > 33 ? 1 : battery > 5 ? 2 : 3;

        if (charging)
            frame = 3 - (int)((t_ms / 600) % 4);
        pw_draw_image(c, b->px + frame * fh * b->w, b->w, fh, 463 - b->w / 2, 12 - fh / 2, alpha);
    } else if (battery >= 0) {
        /* drawn: an outlined cell with three bars, its tip on the right (XMB at 451..474) */
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
    if (ui->style == PW_STYLE_MODERN) {
        pw_modern_hub(c, ui, cat, rows, count, selpos, fade);
        return;
    }
    if (ui->style == PW_STYLE_MINIMAL) {
        pw_minimal_hub(c, ui, cat, rows, count, selpos, fade);
        return;
    }
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

static void choice(pw_canvas *c, const pw_ui *ui, int x, const char *label, int selected, int alpha)
{
    int w = pw_text_width(ui->title, label), bw = w + 8 > 30 ? w + 8 : 30;

    if (selected) /* the HOME screen's light box behind the chosen answer */
        pw_draw_blend(c, x - bw / 2, CHOICE_Y - 12, bw, 16, PW_RGB(0xCF, 0xCF, 0xCF),
                      110 * alpha / 256);
    pw_text_draw(c, ui->title, x - w / 2, CHOICE_Y, label, PW_C_TEXT, alpha, 1);
}

#define DLG_LINES    4
#define DLG_LINE_MAX PW_VIEW_LINE_MAX

/*
 * Break s at spaces into lines no wider than max, from out[n] on, at most up to line `cap`; the
 * last allowed line takes the rest (drawn with "..." if it is still too wide). Returns the new n.
 */
int pw_view_wrap(pw_font *f, const char *s, int max, char (*out)[DLG_LINE_MAX], int n, int cap)
{
    while (*s == ' ')
        s++;
    while (*s && n < cap) {
        char *line = out[n];
        int len = 0;

        if (n == cap - 1) { /* the last line: everything that is left */
            while (s[len] && len < DLG_LINE_MAX - 1) {
                line[len] = s[len];
                len++;
            }
            line[len] = 0;
            return n + 1;
        }
        line[0] = 0;
        for (;;) {
            const char *w = s, *e;
            char tmp[DLG_LINE_MAX];
            int add;

            while (*w == ' ')
                w++;
            for (e = w; *e && *e != ' ';)
                e++;
            if (e == w)
                break;
            add = (int)(e - w) + (len ? 1 : 0);
            if (len + add >= DLG_LINE_MAX)
                break;
            memcpy(tmp, line, (unsigned int)len);
            if (len)
                tmp[len] = ' ';
            memcpy(tmp + len + (len ? 1 : 0), w, (unsigned int)(e - w));
            tmp[len + add] = 0;
            if (len && pw_text_width(f, tmp) > max)
                break; /* a line keeps at least one word */
            memcpy(line, tmp, (unsigned int)(len + add + 1));
            len += add;
            s = e;
        }
        if (len == 0)
            break;
        n++;
        while (*s == ' ')
            s++;
    }
    return n;
}

void pw_view_dialog(pw_canvas *c, const pw_ui *ui, int kind, const char *line1, const char *line2,
                    int yes, int appear)
{
    if (ui->style == PW_STYLE_MODERN) {
        pw_modern_dialog(c, ui, kind, line1, line2, yes, appear);
        return;
    }
    if (ui->style == PW_STYLE_MINIMAL) {
        pw_minimal_dialog(c, ui, kind, line1, line2, yes, appear);
        return;
    }
    char lines[DLG_LINES][DLG_LINE_MAX];
    int max = RULE_X1 - RULE_X0 + 20, n, top;

    /* Like the system's dialogs, long text wraps; the band grows upwards for a third line. */
    n = pw_view_wrap(ui->title, line1, max, lines, 0, line2 ? 2 : 3);
    if (line2)
        n = pw_view_wrap(ui->title, line2, max, lines, n, n + 2 > DLG_LINES ? DLG_LINES : n + 2);
    top = n > 2 ? RULE_TOP - (n - 2) * (TEXT_Y2 - TEXT_Y1) : RULE_TOP;
    pw_draw_blend(c, RULE_X0, top, RULE_X1 - RULE_X0, 1, PW_C_TEXT, appear);
    pw_draw_blend(c, RULE_X0, RULE_BOT, RULE_X1 - RULE_X0, 1, PW_C_TEXT, appear);
    for (int i = 0; i < n; i++) {
        int y = n == 1 ? (TEXT_Y1 + TEXT_Y2) / 2 : TEXT_Y2 - (n - 1 - i) * (TEXT_Y2 - TEXT_Y1);

        pw_view_text_centered(c, ui->title, SCREEN_W / 2, y, lines[i], PW_C_TEXT, appear);
    }
    if (kind == PW_VIEW_CONFIRM) {
        choice(c, ui, YES_X, pw_tr(PW_S_YES), yes, appear);
        choice(c, ui, NO_X, pw_tr(PW_S_NO), !yes, appear);
        pw_view_hints(c, ui, pw_tr(PW_S_ENTER), pw_tr(PW_S_BACK), appear);
    } else {
        choice(c, ui, SCREEN_W / 2, pw_tr(PW_S_OK), 1, appear);
        pw_view_hints(c, ui, pw_tr(PW_S_ENTER), 0, appear);
    }
}

void pw_view_busy(pw_canvas *c, const pw_ui *ui, const char *text, unsigned int t_ms)
{
    if (ui->style == PW_STYLE_MODERN) {
        pw_modern_busy(c, ui, text, t_ms);
        return;
    }
    if (ui->style == PW_STYLE_MINIMAL) {
        pw_minimal_busy(c, ui, text, t_ms);
        return;
    }
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

/* Centre y of a slot icon at p (1/256 slots from the cursor): the chosen one in the frame, the
   others in a column above and below, moving smoothly between those places. */
static int slot_cy(int p)
{
    const int sel_cy = LIST_ICON_Y + 40, below = LIST_ICON_Y + 80 + 6 + LIST_SMALL_H / 2;
    const int above = LIST_ICON_Y - 6 - LIST_SMALL_H / 2, pitch = LIST_SMALL_H + 6;

    if (p >= 256)
        return below + (p - 256) * pitch / 256;
    if (p >= 0)
        return sel_cy + (below - sel_cy) * p / 256;
    if (p >= -256)
        return sel_cy + (sel_cy - above) * p / 256;
    return above + (p + 256) * pitch / 256;
}

static int nearness(int p)
{
    return p >= 256 || p <= -256 ? 0 : 256 - (p < 0 ? -p : p);
}

void pw_view_slots(pw_canvas *c, const pw_ui *ui, const char *heading, const char *game,
                   const pw_slot_row *rows, int count, int selpos, int can_delete, int appear)
{
    if (ui->style == PW_STYLE_MODERN) {
        pw_modern_slots(c, ui, heading, game, rows, count, selpos, can_delete, appear);
        return;
    }
    if (ui->style == PW_STYLE_MINIMAL) {
        pw_minimal_slots(c, ui, heading, game, rows, count, selpos, can_delete, appear);
        return;
    }
    int sel = (selpos + 128) / 256, slide = (256 - appear) * 24 / 256,
        cx = LIST_ICON_X + 72 - slide;
    const pw_slot_row *r;
    int dn, text_a;

    sel = sel < 0 ? 0 : sel >= count ? count - 1 : sel;
    r = &rows[sel];
    /* the details cross-fade while the list moves: gone half way, back when it settles */
    dn = 256 - 2 * ((sel * 256 - selpos) < 0 ? selpos - sel * 256 : sel * 256 - selpos);
    text_a = (dn < 0 ? 0 : dn) * appear / 256;

    for (int i = 0; i < count; i++) {
        int p = i * 256 - selpos, near = nearness(p);
        int w = LIST_SMALL_W + (144 - LIST_SMALL_W) * near / 256;
        int h = LIST_SMALL_H + (80 - LIST_SMALL_H) * near / 256, cy = slot_cy(p);

        if (cy + h / 2 < 0 || cy - h / 2 > SCREEN_H)
            continue;
        slot_icon(c, &rows[i], cx - w / 2, cy - h / 2, w, h,
                  (200 + 56 * near / 256) * appear / 256);
    }
    /* the white frame stays put: the icons pass through it */
    pw_draw_blend(c, cx - 73, LIST_ICON_Y - 1, 146, 1, PW_C_TEXT, appear);
    pw_draw_blend(c, cx - 73, LIST_ICON_Y + 80, 146, 1, PW_C_TEXT, appear);
    pw_draw_blend(c, cx - 73, LIST_ICON_Y, 1, 80, PW_C_TEXT, appear);
    pw_draw_blend(c, cx + 72, LIST_ICON_Y, 1, 80, PW_C_TEXT, appear);
    /* the banner over everything that scrolls under it */
    pw_draw_blend(c, 0, 0, SCREEN_W, 23, PW_RGB(0x58, 0x63, 0x63), 160);
    pw_text_draw(c, ui->title, 30, 17, heading, PW_C_TEXT, 256, 1);

    pw_text_draw_fit(c, ui->title, DETAIL_X, 132, game, SCREEN_W - DETAIL_X - 6, PW_C_GREY, appear,
                     1);
    pw_draw_blend(c, DETAIL_X, 136, SCREEN_W - DETAIL_X, 1, PW_C_TEXT, appear);
    if (r->info)
        pw_text_draw(c, ui->body, DETAIL_X, 152, r->info, PW_C_TEXT, text_a, 1);
    pw_text_draw_fit(c, ui->title, 175, 174, r->title, SCREEN_W - 181, PW_C_TEXT, text_a, 1);
    if (r->exists)
        pw_text_draw(c, ui->body, 175, 192, "Pocketwatch", PW_C_GREY, text_a, 1);

    pw_view_hints(c, ui, pw_tr(PW_S_ENTER), pw_tr(PW_S_BACK), 256);
    if (can_delete && r->exists) {
        int w = pw_text_width(ui->body, pw_tr(PW_S_DELETE)), x = SCREEN_W - 10 - w;

        pw_view_button(c, ui, PW_BTN_TRIANGLE, x - 8, 256, 256);
        pw_text_draw(c, ui->body, x, 260, pw_tr(PW_S_DELETE), PW_C_TEXT, 256, 1);
    }
}

#define SET_Y0     128 /* first option row (baseline), under the category's label */
#define SET_VAL_X  380 /* the values' column (the XMB's option lists)  */
#define SET_PITCH  30
#define POPUP_X    296
#define POPUP_TEXT 312

void pw_view_settings(pw_canvas *c, const pw_ui *ui, const pw_hub_cat *cat,
                      const pw_setting_row *rows, int count, int sel, const char *const *options,
                      int n_options, int opt_sel, int opt_appear)
{
    if (ui->style == PW_STYLE_MODERN) {
        pw_modern_settings(c, ui, cat, rows, count, sel, options, n_options, opt_sel, opt_appear);
        return;
    }
    if (ui->style == PW_STYLE_MINIMAL) {
        pw_minimal_settings(c, ui, cat, rows, count, sel, options, n_options, opt_sel, opt_appear);
        return;
    }
    int editing = opt_sel >= 0 && options && n_options > 0;

    if (cat->icon)
        image_centered(c, cat->icon, CAT_X, CAT_Y, 256);
    else
        pw_icon_draw(c, cat->fallback, CAT_X, CAT_Y, 50, PW_C_TEXT, 256);
    if (cat->label)
        pw_text_draw(c, ui->body, CAT_X - pw_text_width(ui->body, cat->label) / 2, CAT_LABEL_Y,
                     cat->label, PW_C_TEXT, 256, 1);

    for (int i = 0; i < count; i++) {
        int y = SET_Y0 + i * SET_PITCH, alpha = i == sel ? 256 : DIM_ALPHA;
        int max = (editing ? POPUP_X - 8 : SET_VAL_X - 10) - TEXT_X;

        /* like the XMB's option lists (ARK-4's captures): the name, the value in a column of its
           own, a thin line under the chosen row out to the edge */
        pw_text_draw_fit(c, ui->title, TEXT_X, y, rows[i].label, max, PW_C_TEXT, alpha, 1);
        if (!editing && rows[i].value)
            pw_text_draw_fit(c, ui->body, SET_VAL_X, y, rows[i].value, SCREEN_W - SET_VAL_X - 6,
                             PW_C_TEXT, alpha * 3 / 4, 1);
        if (i == sel)
            pw_draw_blend(c, TEXT_X, y + 5, (editing ? POPUP_X : SCREEN_W) - TEXT_X - 6, 1,
                          PW_C_TEXT, 200);
    }

    if (editing) { /* the option panel slides in from the right edge */
        int cy = SET_Y0 + sel * SET_PITCH, a = opt_appear < 0     ? 0
                                               : opt_appear > 256 ? 256
                                                                  : opt_appear;
        int dx = (256 - a) * (SCREEN_W - POPUP_X) / 256;

        pw_draw_blend(c, POPUP_X + dx, 0, SCREEN_W - POPUP_X - dx, SCREEN_H, PW_C_BLACK,
                      150 * a / 256);
        for (int k = 0; k < n_options; k++) {
            int y = cy + (k - opt_sel) * 24;

            if (y < 12 || y > SCREEN_H - 24)
                continue;
            if (k == opt_sel)
                pw_draw_blend(c, POPUP_X + 6 + dx, y - 15, SCREEN_W - POPUP_X - 12, 20,
                              PW_RGB(0xCF, 0xCF, 0xCF), 70 * a / 256);
            pw_text_draw_fit(c, ui->title, POPUP_TEXT + dx, y, options[k],
                             SCREEN_W - POPUP_TEXT - 10, PW_C_TEXT,
                             (k == opt_sel ? 256 : DIM_ALPHA) * a / 256, 1);
        }
    }
    pw_view_hints(c, ui, pw_tr(PW_S_ENTER), pw_tr(PW_S_BACK), 256);
}

void pw_view_speaker(pw_canvas *c, int x, int y, int muted, unsigned int color, int alpha)
{
    /* the box and the cone */
    pw_draw_blend(c, x, y + 5, 4, 6, color, alpha);
    pw_shape_triangle(c, PW_FX(x + 3), PW_FX(y + 5), PW_FX(x + 9), PW_FX(y + 1), PW_FX(x + 9),
                      PW_FX(y + 15), color, alpha);
    pw_shape_triangle(c, PW_FX(x + 3), PW_FX(y + 5), PW_FX(x + 9), PW_FX(y + 15), PW_FX(x + 3),
                      PW_FX(y + 11), color, alpha);
    if (muted) {
        pw_shape_capsule(c, PW_FX(x + 11), PW_FX(y + 5), PW_FX(x + 16), PW_FX(y + 11), PW_FX(1),
                         color, alpha);
        pw_shape_capsule(c, PW_FX(x + 11), PW_FX(y + 11), PW_FX(x + 16), PW_FX(y + 5), PW_FX(1),
                         color, alpha);
    } else {
        pw_shape_capsule(c, PW_FX(x + 12), PW_FX(y + 6), PW_FX(x + 12), PW_FX(y + 10), PW_FX(1),
                         color, alpha);
        pw_shape_capsule(c, PW_FX(x + 15), PW_FX(y + 4), PW_FX(x + 15), PW_FX(y + 12), PW_FX(1),
                         color, alpha);
    }
}

#define VOL_STEP_W   5
#define VOL_STEP_GAP 2
#define VOL_STEP_H   10
#define VOL_BAR_H    24
#define VOL_BAR_Y    (SCREEN_H - VOL_BAR_H - 12)

void pw_view_volume(pw_canvas *c, const pw_ui *ui, const pw_volume *v)
{
    if (ui->style == PW_STYLE_MODERN) {
        pw_modern_volume(c, ui, v);
        return;
    }
    if (ui->style == PW_STYLE_MINIMAL) {
        pw_minimal_volume(c, ui, v);
        return;
    }
    int a = v->alpha, pitch = VOL_STEP_W + VOL_STEP_GAP,
        w = PW_VOLUME_MAX * pitch - VOL_STEP_GAP + 44, x = (SCREEN_W - w) / 2, sx = x + 34,
        sy = VOL_BAR_Y + (VOL_BAR_H - VOL_STEP_H) / 2, full = v->shown / 256, part = v->shown % 256,
        on = v->muted ? 110 : 235;

    if (a <= 0)
        return;
    pw_shape_rrect(c, PW_FX(x), PW_FX(VOL_BAR_Y), PW_FX(w), PW_FX(VOL_BAR_H), PW_FX(4), 0,
                   PW_RGB(0, 0, 0), a * 150 / 256);
    pw_view_speaker(c, x + 10, VOL_BAR_Y + 4, v->muted, PW_C_TEXT, a);
    for (int i = 0; i < PW_VOLUME_MAX; i++) { /* the steps; the one being reached fills in */
        int fill = i < full ? 256 : i == full ? part : 0;

        pw_draw_blend(c, sx + i * pitch, sy, VOL_STEP_W, VOL_STEP_H, PW_C_TEXT, a * 55 / 256);
        if (fill)
            pw_draw_blend(c, sx + i * pitch, sy, VOL_STEP_W, VOL_STEP_H, PW_C_TEXT,
                          a * on / 256 * fill / 256);
    }
    if (v->flash && !v->muted) { /* the step that just changed glows a moment */
        int i = v->up ? v->level - 1 : v->level;

        if (i >= 0 && i < PW_VOLUME_MAX)
            pw_draw_blend(c, sx + i * pitch - 1, sy - 2, VOL_STEP_W + 2, VOL_STEP_H + 4, PW_C_TEXT,
                          a * v->flash / 512);
    }
}

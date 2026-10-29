// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "keyboard.h"

/* The character rows of each page, one key per UTF-8 character (the PSP's own keyboard has
   these pages too: letters, the system language's letters, signs). */
static const char *const k_rows[HM_KB_PAGES][4] = {
    {"1234567890", "qwertyuiop", "asdfghjkl'", "zxcvbnm,.-"},
    {"1234567890", "йцукенгшщзх", "фывапролджэ", "ячсмитьбюъё"},
    {"1234567890", "!@#$%^&*()~", "-_=+[]{}\\|`", ";:'\",.<>/?"},
};
/* Ukrainian: і ї є ґ where Russian has ы ъ э ё (the Ukrainian ЙЦУКЕН) */
static const char *const k_uk[4] = {"1234567890", "йцукенгшщзх", "фівапролджє", "ячсмитьбюїґ"};

/* The last row, in half key widths: Shift, the next page, Space, Erase, Done. */
static const int k_last[] = {HM_KB_SHIFT, HM_KB_PAGE, HM_KB_SPACE, HM_KB_ERASE, HM_KB_DONE};
static const int k_last_w[] = {3, 3, 8, 3, 4};
#define LAST_KEYS 5

static int clen(unsigned char c)
{
    return c < 0x80             ? 1
           : (c & 0xE0) == 0xC0 ? 2
           : (c & 0xF0) == 0xE0 ? 3
           : (c & 0xF8) == 0xF0 ? 4
                                : 1;
}

int hm_kb_chars(const char *s)
{
    int n = 0;

    for (; *s; s += clen((unsigned char)*s))
        n++;
    return n;
}

/* The byte where character i of s starts (its end for i past it). */
static int byte_of(const char *s, int i)
{
    int b = 0;

    while (i-- > 0 && s[b])
        b += clen((unsigned char)s[b]);
    return b;
}

static const char *row_text(const hm_kb *k, int row)
{
    return k->page == HM_KB_CYRILLIC && k->ukrainian ? k_uk[row] : k_rows[k->page][row];
}

void hm_kb_open(hm_kb *k, const char *title, const char *text, int max_chars, int ukrainian)
{
    memset(k, 0, sizeof(*k));
    k->title = title;
    k->max_chars = max_chars;
    k->ukrainian = ukrainian;
    k->row = 1; /* q */
    strncpy(k->text, text, HM_KB_BYTES - 1);
    k->text[byte_of(k->text, max_chars)] = 0;
    strcpy(k->was, k->text);
    k->caret = hm_kb_chars(k->text);
}

int hm_kb_keys(const hm_kb *k, int row)
{
    return row < 4 ? hm_kb_chars(row_text(k, row)) : LAST_KEYS;
}

int hm_kb_special(const hm_kb *k, int row, int col)
{
    (void)k;
    return row < 4 || col < 0 || col >= LAST_KEYS ? HM_KB_CHAR : k_last[col];
}

const char *hm_kb_char(const hm_kb *k, int row, int col, char *out)
{
    const char *s = row_text(k, row), *c = s + byte_of(s, col);
    int n = clen((unsigned char)*c);

    memcpy(out, c, (size_t)n);
    out[n] = 0;
    if (!k->shift)
        return out;
    if (n == 1 && out[0] >= 'a' && out[0] <= 'z') {
        out[0] = (char)(out[0] - 32);
    } else if (n == 2) { /* Cyrillic: а-я, ѐ-џ (ё і ї є), ґ */
        unsigned int cp = ((unsigned int)out[0] & 0x1F) << 6 | ((unsigned int)out[1] & 0x3F);

        cp = cp >= 0x430 && cp <= 0x44F   ? cp - 0x20
             : cp >= 0x450 && cp <= 0x45F ? cp - 0x50
             : cp == 0x491                ? 0x490
                                          : cp;
        out[0] = (char)(0xC0 | cp >> 6);
        out[1] = (char)(0x80 | (cp & 0x3F));
    }
    return out;
}

void hm_kb_place(const hm_kb *k, int row, int col, int *x, int *w)
{
    int total = 0, at = 0;

    if (row < 4) {
        total = 2 * hm_kb_keys(k, row);
        *x = -total / 2 + 2 * col;
        *w = 2;
        return;
    }
    for (int i = 0; i < LAST_KEYS; i++) {
        if (i == col)
            at = total;
        total += k_last_w[i];
    }
    *x = -total / 2 + at;
    *w = k_last_w[col];
}

void hm_kb_move(hm_kb *k, int dx, int dy)
{
    int n = hm_kb_keys(k, k->row);

    if (dx) {
        k->col = (k->col + dx + n) % n;
    } else if (dy) {
        int x, w, c, best = 1 << 30, row = (k->row + dy + HM_KB_ROWS) % HM_KB_ROWS;

        hm_kb_place(k, k->row, k->col, &x, &w);
        c = 2 * x + w; /* (twice its middle) */
        for (int i = 0; i < hm_kb_keys(k, row); i++) {
            int d;

            hm_kb_place(k, row, i, &x, &w);
            d = 2 * x + w - c;
            if (d < 0)
                d = -d;
            if (d < best) {
                best = d;
                k->col = i;
            }
        }
        k->row = row;
    }
}

static void insert(hm_kb *k, const char *s)
{
    int at = byte_of(k->text, k->caret), n = (int)strlen(s), len = (int)strlen(k->text);

    if (hm_kb_chars(k->text) >= k->max_chars || len + n >= HM_KB_BYTES)
        return;
    memmove(k->text + at + n, k->text + at, (size_t)(len - at + 1));
    memcpy(k->text + at, s, (size_t)n);
    k->caret++;
}

void hm_kb_type(hm_kb *k, const char *utf8)
{
    insert(k, utf8);
}

int hm_kb_press(hm_kb *k)
{
    char c[8];

    switch (hm_kb_special(k, k->row, k->col)) {
    case HM_KB_CHAR:
        insert(k, hm_kb_char(k, k->row, k->col, c));
        k->shift = 0; /* one capital, as on a phone */
        return 0;
    case HM_KB_SHIFT:
        k->shift = !k->shift;
        return 0;
    case HM_KB_PAGE:
        hm_kb_next_page(k);
        return 0;
    case HM_KB_SPACE:
        insert(k, " ");
        return 0;
    case HM_KB_ERASE:
        hm_kb_erase(k);
        return 0;
    default:
        return 1; /* Done */
    }
}

void hm_kb_erase(hm_kb *k)
{
    int from, to;

    if (k->caret <= 0)
        return;
    from = byte_of(k->text, k->caret - 1);
    to = byte_of(k->text, k->caret);
    memmove(k->text + from, k->text + to, strlen(k->text + to) + 1);
    k->caret--;
}

void hm_kb_caret(hm_kb *k, int d)
{
    int n = hm_kb_chars(k->text);

    k->caret = k->caret + d < 0 ? 0 : k->caret + d > n ? n : k->caret + d;
}

void hm_kb_next_page(hm_kb *k)
{
    k->page = (k->page + 1) % HM_KB_PAGES;
    if (k->row < 4 && k->col >= hm_kb_keys(k, k->row))
        k->col = hm_kb_keys(k, k->row) - 1;
}

int hm_kb_changed(const hm_kb *k)
{
    return strcmp(k->text, k->was) != 0;
}

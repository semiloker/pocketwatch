// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_KEYBOARD_H
#define HM_KEYBOARD_H

/*
 * Home's own on-screen keyboard (HOM-REQ-030, ADR-039): the firmware's keyboard and its "keep the
 * text?" question looked like the PSP's, not Pocketwatch's (owner, 2026-10-20). Portable (no PSP
 * headers, host-tested): the pages of keys, the text being typed (UTF-8) and the question asked
 * when Back leaves changes. main.c maps the buttons, home_view.c draws it in every style.
 */

#define HM_KB_BYTES 64 /* the text's bytes with its end (the system's nickname holds 64) */
#define HM_KB_ROWS  5  /* four rows of characters, then the row of Shift, Space, ... */

enum { HM_KB_LATIN, HM_KB_CYRILLIC, HM_KB_SYMBOLS, HM_KB_PAGES };

/* The last row's keys (hm_kb_special); 0 is a character. */
enum { HM_KB_CHAR, HM_KB_SHIFT, HM_KB_PAGE, HM_KB_SPACE, HM_KB_ERASE, HM_KB_DONE };

/* The question's answers (ask_sel). */
enum { HM_KB_ASK_SAVE, HM_KB_ASK_DISCARD, HM_KB_ASK_EDIT };

typedef struct {
    char text[HM_KB_BYTES]; /* UTF-8 */
    char was[HM_KB_BYTES];  /* when it opened: Back without changes asks nothing */
    const char *title;
    int max_chars;
    int caret; /* characters before it */
    int page, shift, ukrainian;
    int row, col; /* the key chosen */
    int ask;      /* "save the changes?" is shown */
    int ask_sel;
} hm_kb;

void hm_kb_open(hm_kb *k, const char *title, const char *text, int max_chars, int ukrainian);

int hm_kb_keys(const hm_kb *k, int row);             /* keys in a row */
int hm_kb_special(const hm_kb *k, int row, int col); /* HM_KB_CHAR or the key's kind */
/* A character key's text as typed now (shifted), into out (8 bytes). */
const char *hm_kb_char(const hm_kb *k, int row, int col, char *out);
/* Where a key is, in half key widths around the middle of the keyboard: its left edge and width. */
void hm_kb_place(const hm_kb *k, int row, int col, int *x, int *w);

void hm_kb_move(hm_kb *k, int dx, int dy);   /* the chosen key: along its row (round), up / down
                                                to the nearest key */
int hm_kb_press(hm_kb *k);                   /* Enter on the chosen key: 1 when it was Done */
void hm_kb_type(hm_kb *k, const char *utf8); /* one character at the caret (Triangle: a space) */
void hm_kb_erase(hm_kb *k);                  /* the character before the caret */
void hm_kb_caret(hm_kb *k, int d);
void hm_kb_next_page(hm_kb *k);
int hm_kb_changed(const hm_kb *k);
int hm_kb_chars(const char *utf8); /* characters, not bytes */

#endif

// SPDX-License-Identifier: GPL-3.0-or-later
#include "keyboard.h"
#include "pwtest.h"

PW_TEST(keyboard_types_erases_and_moves_the_caret_in_utf8)
{
    hm_kb k;
    char c[8];

    hm_kb_open(&k, "Nickname", "ab", 5, 0);
    CHECK_EQ(k.caret, 2);
    CHECK(!hm_kb_changed(&k));
    k.page = HM_KB_CYRILLIC;
    k.row = 1;
    k.col = 0; /* й */
    k.shift = 1;
    CHECK_STR(hm_kb_char(&k, 1, 0, c), "Й");
    hm_kb_caret(&k, -1);
    CHECK_EQ(hm_kb_press(&k), 0);
    CHECK_STR(k.text, "aЙb");
    CHECK_EQ(k.shift, 0); /* one capital */
    hm_kb_type(&k, "ё");
    CHECK_STR(k.text, "aЙёb");
    hm_kb_erase(&k);
    hm_kb_erase(&k);
    CHECK_STR(k.text, "ab");
    CHECK_EQ(k.caret, 1);
    hm_kb_type(&k, "x");
    hm_kb_type(&k, "y");
    hm_kb_type(&k, "z");
    hm_kb_type(&k, "!"); /* past max_chars: nothing */
    CHECK_STR(k.text, "axyzb");
    CHECK(hm_kb_changed(&k));
    hm_kb_caret(&k, 99);
    CHECK_EQ(k.caret, 5);
}

PW_TEST(keyboard_shift_covers_ukrainian_and_moves_to_the_nearest_key)
{
    hm_kb k;
    char c[8];
    int x, w;

    hm_kb_open(&k, "", "", 32, 1);
    k.page = HM_KB_CYRILLIC;
    k.shift = 1;
    CHECK_STR(hm_kb_char(&k, 3, 10, c), "Ґ");
    CHECK_STR(hm_kb_char(&k, 2, 1, c), "І");
    CHECK_STR(hm_kb_char(&k, 3, 9, c), "Ї");
    CHECK_STR(hm_kb_char(&k, 2, 10, c), "Є");
    /* down from the far right of the letters: Done, the last row's last key */
    k.row = 3;
    k.col = 10;
    hm_kb_move(&k, 0, 1);
    CHECK_EQ(k.row, 4);
    CHECK_EQ(hm_kb_special(&k, k.row, k.col), HM_KB_DONE);
    /* up from Space lands in the middle; round along the row */
    k.col = 2;
    hm_kb_move(&k, 0, -1);
    hm_kb_place(&k, k.row, k.col, &x, &w);
    CHECK(x >= -2 && x <= 0);
    k.col = 0;
    hm_kb_move(&k, -1, 0);
    CHECK_EQ(k.col, hm_kb_keys(&k, 3) - 1);
    /* a shorter page keeps the chosen key inside its row */
    hm_kb_next_page(&k);
    CHECK(k.col < hm_kb_keys(&k, k.row));
    k.row = 4;
    k.col = 4;
    CHECK_EQ(hm_kb_press(&k), 1); /* Done */
}

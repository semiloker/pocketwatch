// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "music.h"
#include "pwtest.h"

PW_TEST(music_name_splits_title_and_artist)
{
    char t[96], a[64];

    hm_music_name("ms0:/MUSIC/A Pearl - Mitski.mp3", t, sizeof(t), a, sizeof(a));
    CHECK(strcmp(t, "A Pearl") == 0 && strcmp(a, "Mitski") == 0);
    /* only the first " - ": the rest belongs to the artists */
    hm_music_name("Boy's a liar Pt. 2 - PinkPantheress, Ice Spice.mp3", t, sizeof(t), a, sizeof(a));
    CHECK(strcmp(t, "Boy's a liar Pt. 2") == 0 && strcmp(a, "PinkPantheress, Ice Spice") == 0);
    hm_music_name("ms0:/MUSIC/над городом нуар.mp3", t, sizeof(t), a, sizeof(a));
    CHECK(strcmp(t, "над городом нуар") == 0 && a[0] == 0);
    hm_music_name("Spectre - Mixed.MP3", t, 4, a, 3); /* cut to the buffers */
    CHECK(strcmp(t, "Spe") == 0 && strcmp(a, "Mi") == 0);
}

PW_TEST(music_id3_tag_is_skipped)
{
    unsigned char h[10] = {'I', 'D', '3', 3, 0, 0, 0, 0, 0x02, 0x01}; /* syncsafe 0x101 */

    CHECK_EQ(hm_music_id3_size(h), 0x101u + 10);
    h[5] = 0x10; /* a footer */
    CHECK_EQ(hm_music_id3_size(h), 0x101u + 20);
    h[8] = 0x80; /* not syncsafe: not a tag */
    CHECK_EQ(hm_music_id3_size(h), 0u);
    memcpy(h, "\xFF\xFB\x90\x00\0\0\0\0\0\0", 10); /* a frame: no tag */
    CHECK_EQ(hm_music_id3_size(h), 0u);
}

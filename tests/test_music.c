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

/* A tag in memory, read through the callback as the PSP reads a file. */
typedef struct {
    const unsigned char *d;
    unsigned int n;
    int reads;
} mem_file;

static int mem_read(void *ctx, unsigned int off, void *buf, unsigned int n)
{
    mem_file *f = ctx;

    f->reads++;
    if (off >= f->n)
        return 0;
    if (n > f->n - off)
        n = f->n - off;
    memcpy(buf, f->d + off, n);
    return (int)n;
}

static unsigned int frame(unsigned char *p, const char *id, const unsigned char *body,
                          unsigned int n)
{
    memcpy(p, id, 4);
    p[4] = (unsigned char)(n >> 24);
    p[5] = (unsigned char)(n >> 16);
    p[6] = (unsigned char)(n >> 8);
    p[7] = (unsigned char)n;
    p[8] = p[9] = 0;
    memcpy(p + 10, body, n);
    return n + 10;
}

/* v2.3 as the owner's files have it: the words, then a JPEG cover; UTF-16 and Latin-1 text. */
PW_TEST(music_id3_gives_words_and_the_cover)
{
    static unsigned char tag[600];
    static const unsigned char tit2[] = "\x00"
                                        "Blinding Lights";
    static const unsigned char tpe1[] = {1, 0xFF, 0xFE, 'W', 0, 'e',
                                         0, 0x14, 0x04, 0,   0}; /* We Ф */
    static const unsigned char talb[] = "\x00"
                                        "Caf\xE9";
    static const unsigned char apic[] = "\x00"
                                        "image/jpeg\x00\x03"
                                        "desc\x00"
                                        "\xFF\xD8\xFF\xE0JFIF";
    unsigned int n = 10, pic_at;
    mem_file f;
    hm_id3 t;

    memset(tag, 0, sizeof(tag));
    memcpy(tag, "ID3\x03\x00\x00", 6);
    n += frame(tag + n, "TIT2", tit2, sizeof(tit2) - 1);
    n += frame(tag + n, "TPE1", tpe1, sizeof(tpe1));
    n += frame(tag + n, "TALB", talb, sizeof(talb) - 1);
    pic_at = n + 10 + 1 + 11 + 1 + 5;
    n += frame(tag + n, "APIC", apic, sizeof(apic) - 1);
    n += 40; /* the padding */
    tag[6] = 0;
    tag[7] = 0;
    tag[8] = (unsigned char)((n - 10) >> 7);
    tag[9] = (unsigned char)((n - 10) & 0x7F);
    f.d = tag;
    f.n = n;
    f.reads = 0;
    CHECK_EQ(hm_id3_read(mem_read, &f, &t), 0);
    CHECK(strcmp(t.title, "Blinding Lights") == 0);
    CHECK(strcmp(t.artist, "We\xD0\x94") == 0); /* U+0414 in UTF-8 */
    CHECK(strcmp(t.album, "Caf\xC3\xA9") == 0);
    CHECK_EQ(t.pic_off, pic_at);
    CHECK_EQ(t.pic_len, 8u);
    CHECK(tag[t.pic_off] == 0xFF && tag[t.pic_off + 1] == 0xD8);
    CHECK(f.reads < 12); /* a few small reads, not the whole file */
    /* no tag: refused */
    f.d = (const unsigned char *)"\xFF\xFB\x90\x00\0\0\0\0\0\0";
    f.n = 10;
    CHECK(hm_id3_read(mem_read, &f, &t) < 0);
}

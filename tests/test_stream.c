// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdlib.h>

#include "complications/states/stream.h"
#include "core/crc32.h"
#include "pwtest.h"

static unsigned int g_rng = 12345;

static unsigned int rng(void)
{
    g_rng = g_rng * 1103515245u + 12345u;
    return g_rng >> 8;
}

/* Game-RAM-like content: zero pages, repeated structures, some noise. */
static void fill_mixed(unsigned char *p, unsigned int len)
{
    for (unsigned int i = 0; i < len; i++) {
        unsigned int page = i / 4096;
        if (page % 3 == 0)
            p[i] = 0;
        else if (page % 3 == 1)
            p[i] = (unsigned char)("Pocketwatch-state "[i % 18]);
        else
            p[i] = (unsigned char)rng();
    }
}

static void fill_random(unsigned char *p, unsigned int len)
{
    for (unsigned int i = 0; i < len; i++)
        p[i] = (unsigned char)rng();
}

typedef struct {
    pw_arena arena;
    unsigned char *mem[2];
} Arena;

static unsigned int g_workspace[PW_STREAM_WORKSPACE / 4];

static void arena_init(Arena *a, unsigned int size0, unsigned int size1)
{
    pw_stream_set_workspace(g_workspace);
    a->mem[0] = malloc(size0);
    a->mem[1] = size1 ? malloc(size1) : NULL;
    memset(a->mem[0], 0xA5, size0); /* garbage, like real RAM */
    if (a->mem[1])
        memset(a->mem[1], 0x5A, size1);
    a->arena.base[0] = a->mem[0];
    a->arena.size[0] = size0;
    a->arena.base[1] = a->mem[1];
    a->arena.size[1] = size1;
}

static void arena_free(Arena *a)
{
    free(a->mem[0]);
    free(a->mem[1]);
}

static void roundtrip(void (*fill)(unsigned char *, unsigned int), unsigned int len,
                      unsigned int size0, unsigned int size1)
{
    unsigned char *src = malloc(len + 1), *dst = malloc(len + 1);
    pw_arena_pos cur = {0, 0};
    pw_stream s;
    Arena a;

    arena_init(&a, size0, size1);
    fill(src, len);
    CHECK_EQ(pw_stream_write(&a.arena, &cur, src, len, 1, &s), 0);
    CHECK_EQ(s.raw_len, len);
    CHECK_EQ(s.crc32, pw_crc32(0, src, len));
    memset(dst, 0, len + 1);
    CHECK_EQ(pw_stream_read(&a.arena, &s, dst, len), (int)len);
    CHECK(memcmp(src, dst, len) == 0);

    free(src);
    free(dst);
    arena_free(&a);
}

PW_TEST(stream_roundtrip_mixed_multi_chunk)
{
    roundtrip(fill_mixed, 5 * PW_STREAM_CHUNK + 1234, 1 << 20, 0);
}

PW_TEST(stream_roundtrip_incompressible_is_stored_raw)
{
    unsigned char *src = malloc(3 * PW_STREAM_CHUNK);
    pw_arena_pos cur = {0, 0};
    pw_stream s;
    Arena a;

    arena_init(&a, 1 << 20, 0);
    fill_random(src, 3 * PW_STREAM_CHUNK);
    CHECK_EQ(pw_stream_write(&a.arena, &cur, src, 3 * PW_STREAM_CHUNK, 0, &s), 0);
    CHECK_EQ(s.raw_chunks, 3);
    CHECK_EQ(s.crc32, 0); /* not requested */
    free(src);
    arena_free(&a);
    roundtrip(fill_random, 3 * PW_STREAM_CHUNK, 1 << 20, 0);
}

PW_TEST(stream_spills_into_second_region_with_skip_marker)
{
    /* Region 0 holds about one and a half worst-case chunks, so chunk 2 must move to region 1. */
    roundtrip(fill_random, 4 * PW_STREAM_CHUNK, PW_STREAM_CHUNK * 3 / 2, 4 * 80 * 1024);
}

PW_TEST(stream_two_streams_share_the_arena)
{
    unsigned int len1 = 200000, len2 = 70000;
    unsigned char *a1 = malloc(len1), *a2 = malloc(len2), *out = malloc(len1);
    pw_arena_pos cur = {0, 0};
    pw_stream s1, s2;
    Arena a;

    arena_init(&a, 256 * 1024, 256 * 1024);
    fill_mixed(a1, len1);
    fill_random(a2, len2);
    CHECK_EQ(pw_stream_write(&a.arena, &cur, a1, len1, 1, &s1), 0);
    CHECK_EQ(pw_stream_write(&a.arena, &cur, a2, len2, 1, &s2), 0);
    CHECK_EQ(pw_stream_read(&a.arena, &s1, out, len1), (int)len1);
    CHECK(memcmp(out, a1, len1) == 0);
    CHECK_EQ(pw_stream_read(&a.arena, &s2, out, len1), (int)len2);
    CHECK(memcmp(out, a2, len2) == 0);
    free(a1);
    free(a2);
    free(out);
    arena_free(&a);
}

PW_TEST(stream_full_arena_fails_cleanly)
{
    unsigned char *src = malloc(4 * PW_STREAM_CHUNK);
    pw_arena_pos cur = {0, 0};
    pw_stream s;
    Arena a;

    arena_init(&a, PW_STREAM_CHUNK * 2, 0);
    fill_random(src, 4 * PW_STREAM_CHUNK);
    CHECK_EQ(pw_stream_write(&a.arena, &cur, src, 4 * PW_STREAM_CHUNK, 0, &s), -1);
    free(src);
    arena_free(&a);
}

PW_TEST(stream_rejects_corruption_and_small_output)
{
    unsigned int len = 2 * PW_STREAM_CHUNK;
    unsigned char *src = malloc(len), *dst = malloc(len);
    pw_arena_pos cur = {0, 0};
    pw_stream s;
    Arena a;

    arena_init(&a, 1 << 20, 0);
    fill_mixed(src, len);
    CHECK_EQ(pw_stream_write(&a.arena, &cur, src, len, 0, &s), 0);
    CHECK_EQ(pw_stream_read(&a.arena, &s, dst, len - 1), -1); /* output too small */

    a.mem[0][s.start.offset] = 0x7F; /* first record's raw_len becomes nonsense */
    a.mem[0][s.start.offset + 1] = 0x7F;
    CHECK_EQ(pw_stream_read(&a.arena, &s, dst, len), -1);
    free(src);
    free(dst);
    arena_free(&a);
}

PW_TEST(stream_empty_input)
{
    pw_arena_pos cur = {0, 0};
    pw_stream s;
    unsigned char dst[4];
    Arena a;

    arena_init(&a, 4096, 0);
    CHECK_EQ(pw_stream_write(&a.arena, &cur, dst, 0, 1, &s), 0);
    CHECK_EQ(s.chunks, 0);
    CHECK_EQ(pw_stream_read(&a.arena, &s, dst, sizeof(dst)), 0);
    arena_free(&a);
}

PW_TEST(stream_requires_workspace)
{
    unsigned char src[16] = {0};
    pw_arena_pos cur = {0, 0};
    pw_stream s;
    Arena a;

    arena_init(&a, 4096, 0);
    pw_stream_set_workspace(NULL);
    CHECK_EQ(pw_stream_write(&a.arena, &cur, src, sizeof(src), 0, &s), -1);
    pw_stream_set_workspace(g_workspace);
    CHECK_EQ(pw_stream_write(&a.arena, &cur, src, sizeof(src), 0, &s), 0);
    arena_free(&a);
}

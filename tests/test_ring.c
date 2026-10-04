// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/ring.h"
#include "pwtest.h"

PW_TEST(ring_push_pop_in_order)
{
    char buf[8], out[8];
    pw_ring r;

    pw_ring_init(&r, buf, sizeof(buf));
    CHECK_EQ(pw_ring_push(&r, "abc", 3), 0);
    CHECK_EQ(pw_ring_push(&r, "de", 2), 0);
    CHECK_EQ(r.len, 5);
    CHECK_EQ(pw_ring_pop(&r, out, sizeof(out)), 5);
    CHECK(memcmp(out, "abcde", 5) == 0);
    CHECK_EQ(r.len, 0);
}

PW_TEST(ring_wraps_around)
{
    char buf[8], out[8];
    pw_ring r;

    pw_ring_init(&r, buf, sizeof(buf));
    pw_ring_push(&r, "123456", 6);
    CHECK_EQ(pw_ring_pop(&r, out, 4), 4);      /* tail moves to 4, head stays at 6 */
    CHECK_EQ(pw_ring_push(&r, "ABCDE", 5), 0); /* spans the end of the buffer */
    CHECK_EQ(r.len, 7);
    CHECK_EQ(pw_ring_pop(&r, out, sizeof(out)), 7);
    CHECK(memcmp(out, "56ABCDE", 7) == 0);
}

PW_TEST(ring_rejects_whole_record_when_full)
{
    char buf[4], out[4];
    pw_ring r;

    pw_ring_init(&r, buf, sizeof(buf));
    CHECK_EQ(pw_ring_push(&r, "abc", 3), 0);
    CHECK_EQ(pw_ring_push(&r, "de", 2), -1);
    CHECK_EQ(r.dropped, 1);
    CHECK_EQ(r.len, 3); /* nothing partial was written */
    CHECK_EQ(pw_ring_pop(&r, out, sizeof(out)), 3);
    CHECK(memcmp(out, "abc", 3) == 0);
}

PW_TEST(ring_exact_fill_and_partial_pop)
{
    char buf[4], out[4];
    pw_ring r;

    pw_ring_init(&r, buf, sizeof(buf));
    CHECK_EQ(pw_ring_push(&r, "wxyz", 4), 0);
    CHECK_EQ(pw_ring_pop(&r, out, 1), 1);
    CHECK_EQ(out[0], 'w');
    CHECK_EQ(pw_ring_push(&r, "!", 1), 0);
    CHECK_EQ(pw_ring_pop(&r, out, sizeof(out)), 4);
    CHECK(memcmp(out, "xyz!", 4) == 0);
}

PW_TEST(ring_empty_and_zero_length_are_noops)
{
    char buf[4], out[4];
    pw_ring r;

    pw_ring_init(&r, buf, sizeof(buf));
    CHECK_EQ(pw_ring_pop(&r, out, sizeof(out)), 0);
    CHECK_EQ(pw_ring_push(&r, "", 0), 0);
    CHECK_EQ(r.len, 0);
    CHECK_EQ(r.dropped, 0);
}

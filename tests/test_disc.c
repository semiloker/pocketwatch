// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "disc.h"
#include "pwtest.h"

/* A synthetic ISO: PVD in sector 16, the root in 18, PSP_GAME/ in 19, PARAM.SFO in 20. */
#define SECTORS 22
static unsigned char g_iso[SECTORS * HM_SECTOR];
static unsigned char g_cso[24 + 4 * (SECTORS + 1) + SECTORS * HM_SECTOR];
static int g_inflates;

static void put32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

static int record(unsigned char *at, const char *name, unsigned int lba, unsigned int size, int dir)
{
    int n = (int)strlen(name), len = 33 + n + ((33 + n) & 1);

    memset(at, 0, (size_t)len);
    at[0] = (unsigned char)len;
    put32(at + 2, lba);
    put32(at + 10, size);
    at[25] = dir ? 2 : 0;
    at[32] = (unsigned char)n;
    memcpy(at + 33, name, (size_t)n);
    return len;
}

static void make_iso(void)
{
    unsigned char *pvd = g_iso + 16 * HM_SECTOR, *root = g_iso + 18 * HM_SECTOR,
                  *game = g_iso + 19 * HM_SECTOR;
    int o;

    memset(g_iso, 0, sizeof(g_iso));
    pvd[0] = 1;
    memcpy(pvd + 1, "CD001", 5);
    put32(pvd + 80, SECTORS);
    record(pvd + 156, "\0", 18, HM_SECTOR, 1);
    o = record(root, "\0", 18, HM_SECTOR, 1);
    o += record(root + o, "\1", 18, HM_SECTOR, 1);
    record(root + o, "PSP_GAME", 19, HM_SECTOR, 1);
    o = record(game, "\0", 19, HM_SECTOR, 1);
    o += record(game + o, "\1", 18, HM_SECTOR, 1);
    o += record(game + o, "ICON0.PNG;1", 21, 4, 0);
    record(game + o, "PARAM.SFO;1", 20, 9, 0);
    memcpy(g_iso + 20 * HM_SECTOR, "SFO-BYTES", 9);
    memcpy(g_iso + 21 * HM_SECTOR, "PNG!", 4);
}

/* The same image as a CSO: odd blocks stored, even ones "deflated" (the fake inflate copies). */
static void make_cso(void)
{
    unsigned int pos = 24 + 4 * (SECTORS + 1);

    memset(g_cso, 0, sizeof(g_cso));
    memcpy(g_cso, "CISO", 4);
    put32(g_cso + 4, 24);
    put32(g_cso + 8, SECTORS * HM_SECTOR);
    put32(g_cso + 16, HM_SECTOR);
    g_cso[20] = 1;
    g_cso[21] = 0; /* align */
    for (int b = 0; b <= SECTORS; b++) {
        put32(g_cso + 24 + 4 * b, pos | (b % 2 && b < SECTORS ? 0x80000000u : 0));
        if (b < SECTORS) {
            memcpy(g_cso + pos, g_iso + b * HM_SECTOR, HM_SECTOR);
            pos += HM_SECTOR;
        }
    }
}

static int read_mem(void *ctx, unsigned int offset, void *buf, unsigned int len)
{
    const unsigned char *img = ctx;
    unsigned int size = img == g_iso ? sizeof(g_iso) : sizeof(g_cso);

    if (offset >= size)
        return 0;
    if (len > size - offset)
        len = size - offset;
    memcpy(buf, img + offset, len);
    return (int)len;
}

static int fake_inflate(void *out, int out_len, const void *in, int in_len)
{
    g_inflates++;
    if (in_len < out_len)
        return -1;
    memcpy(out, in, (size_t)out_len);
    return out_len;
}

static void check_image(hm_disc *d)
{
    unsigned int off, size;
    char buf[16];

    CHECK_EQ(hm_disc_find(d, "PSP_GAME/PARAM.SFO", &off, &size), 0);
    CHECK_EQ(off, 20u * HM_SECTOR);
    CHECK_EQ(size, 9u);
    CHECK_EQ(hm_disc_read(d, off, buf, size), 9);
    CHECK(memcmp(buf, "SFO-BYTES", 9) == 0);
    CHECK_EQ(hm_disc_find(d, "psp_game/icon0.png", &off, &size), 0); /* any case */
    CHECK_EQ(hm_disc_read(d, off, buf, size), 4);
    CHECK(memcmp(buf, "PNG!", 4) == 0);
    CHECK(hm_disc_find(d, "PSP_GAME/PIC1.PNG", &off, &size) < 0);
    CHECK(hm_disc_find(d, "SYSDIR/EBOOT.BIN", &off, &size) < 0);
}

PW_TEST(disc_finds_files_in_an_iso)
{
    hm_disc d;

    make_iso();
    CHECK_EQ(hm_disc_open(&d, read_mem, fake_inflate, g_iso), 0);
    CHECK_EQ(d.cso, 0);
    CHECK_EQ(d.size, (unsigned int)SECTORS * HM_SECTOR);
    check_image(&d);
}

PW_TEST(disc_reads_a_cso_through_its_index)
{
    hm_disc d;
    unsigned char across[100];

    make_iso();
    make_cso();
    g_inflates = 0;
    CHECK_EQ(hm_disc_open(&d, read_mem, fake_inflate, g_cso), 0);
    CHECK_EQ(d.cso, 1);
    check_image(&d);
    CHECK(g_inflates > 0); /* the even blocks went through inflate */
    /* a read across two blocks */
    CHECK_EQ(hm_disc_read(&d, 21 * HM_SECTOR - 50, across, 100), 100);
    CHECK(memcmp(across + 50, "PNG!", 4) == 0);
}

PW_TEST(disc_refuses_what_is_not_a_game_image)
{
    hm_disc d;

    memset(g_iso, 0, sizeof(g_iso));
    CHECK(hm_disc_open(&d, read_mem, fake_inflate, g_iso) < 0);
}

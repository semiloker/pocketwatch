// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "disc.h"

/*
 * CSO (CISO v1): "CISO", u32 header size, u64 total bytes, u32 block size, u8 version, u8 align,
 * 2 reserved; then (blocks + 1) u32 index entries: block i is at (index[i] & 0x7FFFFFFF) << align,
 * up to the next entry; bit 31 set means stored, not deflated. ISO 9660: the primary volume
 * descriptor in sector 16 holds the root directory's record at byte 156.
 */

static unsigned int le32(const unsigned char *p)
{
    return (unsigned int)p[0] | (unsigned int)p[1] << 8 | (unsigned int)p[2] << 16 |
           (unsigned int)p[3] << 24;
}

static int raw(hm_disc *d, unsigned int offset, void *buf, unsigned int len)
{
    int n = d->read(d->ctx, offset, buf, len);

    return n == (int)len ? 0 : -1;
}

static int cso_block(hm_disc *d, unsigned int block)
{
    unsigned char idx[8];
    unsigned int a, b, pos, len;

    if ((int)block == d->cached)
        return 0;
    if (raw(d, 24 + 4 * block, idx, 8) < 0)
        return -1;
    a = le32(idx);
    b = le32(idx + 4);
    pos = (a & 0x7FFFFFFFu) << d->align;
    len = ((b & 0x7FFFFFFFu) << d->align) - pos;
    if (a & 0x80000000u) { /* stored */
        if (raw(d, pos, d->out, HM_SECTOR) < 0)
            return -1;
    } else {
        if (len > sizeof(d->in))
            len = sizeof(d->in);
        if (d->read(d->ctx, pos, d->in, len) <= 0 || !d->inflate ||
            d->inflate(d->out, HM_SECTOR, d->in, (int)len) != HM_SECTOR)
            return -1;
    }
    d->cached = (int)block;
    return 0;
}

int hm_disc_read(hm_disc *d, unsigned int offset, void *buf, unsigned int len)
{
    unsigned char *out = buf;
    unsigned int done = 0;

    if (offset >= d->size)
        return 0;
    if (len > d->size - offset)
        len = d->size - offset;
    if (!d->cso)
        return raw(d, offset, buf, len) < 0 ? -1 : (int)len;
    while (done < len) {
        unsigned int at = offset + done, in = at % HM_SECTOR, n = HM_SECTOR - in;

        if (n > len - done)
            n = len - done;
        if (cso_block(d, at / HM_SECTOR) < 0)
            return -1;
        memcpy(out + done, d->out + in, n);
        done += n;
    }
    return (int)done;
}

int hm_disc_open(hm_disc *d, hm_read_fn read, hm_inflate_fn inflate, void *ctx)
{
    unsigned char h[24], pvd[8];

    memset(d, 0, sizeof(*d));
    d->read = read;
    d->inflate = inflate;
    d->ctx = ctx;
    d->cached = -1;
    if (raw(d, 0, h, sizeof(h)) < 0)
        return -1;
    if (memcmp(h, "CISO", 4) == 0) {
        if (le32(h + 12) != 0 || le32(h + 16) != HM_SECTOR) /* < 4 GB, 2 KB blocks */
            return -1;
        d->cso = 1;
        d->size = le32(h + 8);
        d->align = h[21];
    } else {
        d->size = 0xFFFFFFFFu; /* until the descriptor says */
    }
    if (hm_disc_read(d, 16 * HM_SECTOR, pvd, sizeof(pvd)) != (int)sizeof(pvd) || pvd[0] != 1 ||
        memcmp(pvd + 1, "CD001", 5) != 0)
        return -1;
    if (!d->cso) {
        unsigned char blocks[4];

        if (hm_disc_read(d, 16 * HM_SECTOR + 80, blocks, 4) != 4)
            return -1;
        d->size = le32(blocks) * HM_SECTOR;
    }
    return 0;
}

static int same_name(const unsigned char *name, int len, const char *want, int wlen)
{
    if (len >= 2 && name[len - 2] == ';') /* "PARAM.SFO;1" */
        len -= 2;
    if (len != wlen)
        return 0;
    for (int i = 0; i < len; i++) {
        int a = name[i], b = (unsigned char)want[i];

        if (a >= 'a' && a <= 'z')
            a -= 32;
        if (b >= 'a' && b <= 'z')
            b -= 32;
        if (a != b)
            return 0;
    }
    return 1;
}

int hm_disc_find(hm_disc *d, const char *path, unsigned int *offset, unsigned int *size)
{
    unsigned char rec[34], sector[HM_SECTOR];
    unsigned int dir, dir_len;

    if (hm_disc_read(d, 16 * HM_SECTOR + 156, rec, sizeof(rec)) != (int)sizeof(rec))
        return -1;
    dir = le32(rec + 2);
    dir_len = le32(rec + 10);
    while (*path) {
        const char *end = strchr(path, '/');
        int wlen = end ? (int)(end - path) : (int)strlen(path), found = 0;

        for (unsigned int s = 0; s < (dir_len + HM_SECTOR - 1) / HM_SECTOR && !found; s++) {
            if (hm_disc_read(d, (dir + s) * HM_SECTOR, sector, HM_SECTOR) != HM_SECTOR)
                return -1;
            for (unsigned int i = 0; i + 33 < HM_SECTOR;) {
                unsigned int rl = sector[i];

                if (rl == 0 || i + rl > HM_SECTOR)
                    break; /* the rest of this sector is padding */
                if (rl >= 34 && i + 33 + sector[i + 32] <= HM_SECTOR &&
                    same_name(sector + i + 33, sector[i + 32], path, wlen)) {
                    dir = le32(sector + i + 2);
                    dir_len = le32(sector + i + 10);
                    found = 1;
                    break;
                }
                i += rl;
            }
        }
        if (!found)
            return -1;
        path += wlen;
        if (*path == '/')
            path++;
    }
    *offset = dir * HM_SECTOR;
    *size = dir_len;
    return 0;
}

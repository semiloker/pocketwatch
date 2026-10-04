// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "complications/states/stream.h"
#include "core/crc32.h"
#include "fastlz/fastlz.h"

/* Provided by the vendored FastLZ, built with FASTLZ_EXTERNAL_HTAB. */
extern unsigned int *fastlz_htab;

#define RECORD_HEADER 8
#define SKIP_MARKER   0xFFFFFFFFu /* raw_len of "continue in the next region" */
#define ALIGN4(x)     (((x) + 3u) & ~3u)

/* FastLZ may expand incompressible input by up to ~5%, with a 66-byte minimum buffer. */
static unsigned int worst_case(unsigned int raw)
{
    return raw + raw / 16 + 66;
}

static void put32(unsigned char *p, unsigned int v)
{
    memcpy(p, &v, 4);
}

static unsigned int get32(const unsigned char *p)
{
    unsigned int v;

    memcpy(&v, p, 4);
    return v;
}

static unsigned int room(const pw_arena *a, const pw_arena_pos *c)
{
    if (a->base[c->region] == NULL || a->size[c->region] < c->offset)
        return 0;
    return a->size[c->region] - c->offset;
}

/*
 * Make sure a whole worst-case record fits at the cursor. Otherwise leave a skip marker (when a
 * header still fits) and continue at the start of the next region.
 */
static unsigned char *reserve(const pw_arena *a, pw_arena_pos *c, unsigned int raw)
{
    unsigned int need = RECORD_HEADER + ALIGN4(worst_case(raw));

    while (c->region < PW_ARENA_REGIONS) {
        if (room(a, c) >= need)
            return a->base[c->region] + c->offset;
        if (room(a, c) >= RECORD_HEADER) {
            put32(a->base[c->region] + c->offset, SKIP_MARKER);
            put32(a->base[c->region] + c->offset + 4, 0);
        }
        c->region++;
        c->offset = 0;
    }
    return NULL;
}

void pw_stream_set_workspace(void *workspace)
{
    fastlz_htab = workspace;
}

int pw_stream_write(const pw_arena *a, pw_arena_pos *cursor, const void *src, unsigned int len,
                    int with_crc, pw_stream *out)
{
    const unsigned char *p = src;
    unsigned int done = 0;

    memset(out, 0, sizeof(*out));
    out->raw_len = len;
    if (fastlz_htab == NULL)
        return -1;

    while (done < len) {
        unsigned int raw = len - done < PW_STREAM_CHUNK ? len - done : PW_STREAM_CHUNK;
        unsigned char *rec = reserve(a, cursor, raw);
        int packed;

        if (rec == NULL)
            return -1;
        if (out->chunks == 0)
            out->start = *cursor;

        packed = fastlz_compress_level(1, p + done, (int)raw, rec + RECORD_HEADER);
        if (packed <= 0 || (unsigned int)packed >= raw) {
            memcpy(rec + RECORD_HEADER, p + done, raw);
            packed = (int)raw;
            out->raw_chunks++;
        }
        put32(rec, raw);
        put32(rec + 4, (unsigned int)packed);

        cursor->offset += RECORD_HEADER + ALIGN4((unsigned int)packed);
        out->packed_len += RECORD_HEADER + ALIGN4((unsigned int)packed);
        out->chunks++;
        if (with_crc)
            out->crc32 = pw_crc32(out->crc32, p + done, raw);
        done += raw;
    }
    if (out->chunks == 0)
        out->start = *cursor;
    return 0;
}

int pw_stream_read(const pw_arena *a, const pw_stream *s, void *dst, unsigned int cap)
{
    unsigned char *out = dst;
    pw_arena_pos c = s->start;
    unsigned int done = 0;

    if (s->raw_len > cap)
        return -1;
    for (unsigned int i = 0; i < s->chunks;) {
        const unsigned char *rec;
        unsigned int raw, packed;

        if (c.region >= PW_ARENA_REGIONS)
            return -1;
        if (room(a, &c) < RECORD_HEADER) { /* writer could not even leave a marker here */
            c.region++;
            c.offset = 0;
            continue;
        }
        rec = a->base[c.region] + c.offset;
        raw = get32(rec);
        packed = get32(rec + 4);
        if (raw == SKIP_MARKER) {
            c.region++;
            c.offset = 0;
            continue;
        }
        if (raw == 0 || raw > PW_STREAM_CHUNK || packed == 0 || packed > raw ||
            done + raw > s->raw_len || room(a, &c) - RECORD_HEADER < packed)
            return -1;

        if (packed == raw) {
            memcpy(out + done, rec + RECORD_HEADER, raw);
        } else if (fastlz_decompress(rec + RECORD_HEADER, (int)packed, out + done, (int)raw) !=
                   (int)raw) {
            return -1;
        }
        c.offset += RECORD_HEADER + ALIGN4(packed);
        done += raw;
        i++;
    }
    return done == s->raw_len ? (int)done : -1;
}

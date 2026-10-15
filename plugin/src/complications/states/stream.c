// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "complications/states/stream.h"
#include "core/crc32.h"
#include "core/lz.h"

#define RECORD_HEADER 8
#define SKIP_MARKER   0xFFFFFFFFu /* raw_len of "continue in the next region" */
#define FILL_FLAG     0x80000000u /* stored_len flag: the chunk is one 32-bit word repeated */
#define ALIGN4(x)     (((x) + 3u) & ~3u)

static void *g_workspace;

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
 * Make sure a whole worst-case record (header + the raw chunk) fits at the cursor. Otherwise
 * leave a skip marker (when a header still fits) and continue at the start of the next region.
 */
static unsigned char *reserve(const pw_arena *a, pw_arena_pos *c, unsigned int raw)
{
    unsigned int need = RECORD_HEADER + ALIGN4(raw);

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

/* 1 when the chunk is one 32-bit word repeated (zero pages, cleared buffers): a FILL record.
   Real data differs within the first few words, so the scan costs almost nothing. */
static int uniform_word(const unsigned char *p, unsigned int len, unsigned int *word)
{
    const unsigned int *w = (const unsigned int *)p;

    if (((unsigned long)p & 3u) != 0 || (len & 3u) != 0 || len == 0)
        return 0;
    for (unsigned int i = 1; i < len / 4; i++)
        if (w[i] != w[0])
            return 0;
    *word = w[0];
    return 1;
}

static void fill_words(unsigned char *p, unsigned int len, unsigned int word)
{
    unsigned int *w = (unsigned int *)p;

    for (unsigned int i = 0; i < len / 4; i++)
        w[i] = word;
}

void pw_stream_set_workspace(void *workspace)
{
    g_workspace = workspace;
}

int pw_stream_write(const pw_arena *a, pw_arena_pos *cursor, const void *src, unsigned int len,
                    int with_crc, pw_stream *out)
{
    const unsigned char *p = src;
    unsigned int done = 0;

    memset(out, 0, sizeof(*out));
    out->raw_len = len;
    if (g_workspace == NULL || pw_lz_state_size() > PW_STREAM_WORKSPACE)
        return -1;

    while (done < len) {
        unsigned int raw = len - done < PW_STREAM_CHUNK ? len - done : PW_STREAM_CHUNK;
        unsigned char *rec = reserve(a, cursor, raw);
        unsigned int stored, word;

        if (rec == NULL)
            return -1;
        if (out->chunks == 0)
            out->start = *cursor;

        if (uniform_word(p + done, raw, &word)) {
            put32(rec + RECORD_HEADER, word);
            stored = FILL_FLAG | 4u;
            out->fill_chunks++;
        } else {
            /* Only worth it when strictly smaller: otherwise LZ4 gives up and it is stored raw. */
            int packed =
                pw_lz_pack(p + done, (int)raw, rec + RECORD_HEADER, (int)raw - 1, g_workspace);

            if (packed <= 0) {
                memcpy(rec + RECORD_HEADER, p + done, raw);
                packed = (int)raw;
                out->raw_chunks++;
            }
            stored = (unsigned int)packed;
        }
        put32(rec, raw);
        put32(rec + 4, stored);

        cursor->offset += RECORD_HEADER + ALIGN4(stored & ~FILL_FLAG);
        out->packed_len += RECORD_HEADER + ALIGN4(stored & ~FILL_FLAG);
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
        unsigned int raw, stored, packed;

        if (c.region >= PW_ARENA_REGIONS)
            return -1;
        if (room(a, &c) < RECORD_HEADER) { /* writer could not even leave a marker here */
            c.region++;
            c.offset = 0;
            continue;
        }
        rec = a->base[c.region] + c.offset;
        raw = get32(rec);
        stored = get32(rec + 4);
        if (raw == SKIP_MARKER) {
            c.region++;
            c.offset = 0;
            continue;
        }
        packed = stored & ~FILL_FLAG;
        if (raw == 0 || raw > PW_STREAM_CHUNK || packed == 0 || packed > raw ||
            done + raw > s->raw_len || room(a, &c) - RECORD_HEADER < packed)
            return -1;

        if (stored & FILL_FLAG) {
            if (packed != 4 || (raw & 3u) != 0 || ((unsigned long)(out + done) & 3u) != 0)
                return -1;
            fill_words(out + done, raw, get32(rec + RECORD_HEADER));
        } else if (packed == raw) {
            memcpy(out + done, rec + RECORD_HEADER, raw);
        } else if (pw_lz_unpack(rec + RECORD_HEADER, (int)packed, out + done, (int)raw) !=
                   (int)raw) {
            return -1;
        }
        c.offset += RECORD_HEADER + ALIGN4(packed);
        done += raw;
        i++;
    }
    return done == s->raw_len ? (int)done : -1;
}

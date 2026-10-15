// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_STREAM_H
#define PW_STREAM_H

/*
 * Compressed streams for savestates. Data is cut into 64 KB chunks; each chunk is FastLZ
 * compressed, or stored raw when that would not shrink it. Chunks are written into an arena
 * made of up to two separate memory regions (a region may be absent: base NULL, size 0). A chunk
 * never straddles two regions.
 *
 * Chunk record: u32 raw_len, u32 stored_len (== raw_len means raw), payload, padding to 4 bytes.
 */

#define PW_STREAM_CHUNK     0x10000u
#define PW_STREAM_WORKSPACE 0x8000u /* FastLZ hash table; must not live on a kernel stack */
#define PW_ARENA_REGIONS    2

typedef struct {
    unsigned char *base[PW_ARENA_REGIONS];
    unsigned int size[PW_ARENA_REGIONS];
} pw_arena;

typedef struct {
    unsigned int region;
    unsigned int offset;
} pw_arena_pos;

typedef struct {
    pw_arena_pos start;
    unsigned int raw_len;
    unsigned int packed_len; /* arena bytes used, headers and padding included */
    unsigned int chunks;
    unsigned int raw_chunks; /* chunks stored uncompressed */
    unsigned int crc32;      /* of the raw data, 0 when not computed */
} pw_stream;

/* Workspace for compression (PW_STREAM_WORKSPACE bytes, 4-byte aligned). Required before writing.
 */
void pw_stream_set_workspace(void *workspace);

/*
 * Compress len bytes of src at *cursor, advancing it. With_crc also checksums the raw data.
 * Returns 0, or -1 when the arena is full (the stream is then unusable).
 */
int pw_stream_write(const pw_arena *a, pw_arena_pos *cursor, const void *src, unsigned int len,
                    int with_crc, pw_stream *out);

/* Decompress a stream into dst (cap bytes). Returns raw_len, or -1 on corrupt data. */
int pw_stream_read(const pw_arena *a, const pw_stream *s, void *dst, unsigned int cap);

#endif

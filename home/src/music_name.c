// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "music.h"

static void copy(char *out, int cap, const char *s, int n)
{
    if (n > cap - 1)
        n = cap - 1;
    if (n < 0)
        n = 0;
    memcpy(out, s, (size_t)n);
    out[n] = 0;
}

void hm_music_name(const char *file, char *title, int tcap, char *artist, int acap)
{
    const char *slash = strrchr(file, '/'), *name = slash ? slash + 1 : file,
               *dot = strrchr(name, '.'), *end = dot ? dot : name + strlen(name), *dash = 0;

    for (const char *p = name; p + 3 <= end; p++) /* the first " - " splits title and artist */
        if (p[0] == ' ' && p[1] == '-' && p[2] == ' ') {
            dash = p;
            break;
        }
    if (dash) {
        copy(title, tcap, name, (int)(dash - name));
        copy(artist, acap, dash + 3, (int)(end - dash - 3));
    } else {
        copy(title, tcap, name, (int)(end - name));
        artist[0] = 0;
    }
}

unsigned int hm_music_id3_size(const unsigned char *h)
{
    unsigned int n;

    if (h[0] != 'I' || h[1] != 'D' || h[2] != '3' || (h[6] | h[7] | h[8] | h[9]) & 0x80)
        return 0;
    n = (unsigned int)h[6] << 21 | (unsigned int)h[7] << 14 | (unsigned int)h[8] << 7 | h[9];
    return n + 10 + (h[5] & 0x10 ? 10 : 0); /* the header, and a footer when there is one */
}

/* ---- ID3v2 tags (id3.org's v2.2, v2.3 and v2.4 structure documents) ---- */

static unsigned int be32(const unsigned char *p)
{
    return (unsigned int)p[0] << 24 | (unsigned int)p[1] << 16 | (unsigned int)p[2] << 8 | p[3];
}

static unsigned int syncsafe(const unsigned char *p)
{
    return (unsigned int)(p[0] & 0x7F) << 21 | (unsigned int)(p[1] & 0x7F) << 14 |
           (unsigned int)(p[2] & 0x7F) << 7 | (p[3] & 0x7F);
}

/* One code point into UTF-8 at out[*k] (room for it checked). */
static void put(char *out, int cap, int *k, unsigned int c)
{
    if (c < 0x80 && *k + 1 < cap) {
        out[(*k)++] = (char)c;
    } else if (c < 0x800 && *k + 2 < cap) {
        out[(*k)++] = (char)(0xC0 | c >> 6);
        out[(*k)++] = (char)(0x80 | (c & 0x3F));
    } else if (c >= 0x800 && *k + 3 < cap) {
        out[(*k)++] = (char)(0xE0 | c >> 12);
        out[(*k)++] = (char)(0x80 | ((c >> 6) & 0x3F));
        out[(*k)++] = (char)(0x80 | (c & 0x3F));
    }
}

void hm_id3_text(const unsigned char *s, unsigned int n, char *out, int cap)
{
    int k = 0, enc = n ? s[0] : 0, big = 1;
    unsigned int i = 1;

    if (enc == 1 && n >= 3) { /* UTF-16 with its byte order mark */
        big = !(s[1] == 0xFF && s[2] == 0xFE);
        i = 3;
    }
    while (i < n && k < cap - 1) {
        unsigned int c;

        if (enc == 1 || enc == 2) {
            if (i + 1 >= n)
                break;
            c = big ? (unsigned int)s[i] << 8 | s[i + 1] : (unsigned int)s[i + 1] << 8 | s[i];
            i += 2;
            if (c >= 0xD800 && c < 0xE000) /* (beyond the plane the fonts draw) */
                c = '?';
        } else if (enc == 3) { /* UTF-8 as it is */
            c = s[i++];
            if (c == 0)
                break;
            if (k < cap - 1)
                out[k++] = (char)c;
            continue;
        } else { /* ISO-8859-1 */
            c = s[i++];
        }
        if (c == 0)
            break;
        put(out, cap, &k, c);
    }
    while (k > 0 && out[k - 1] == ' ')
        k--;
    out[k] = 0;
}

int hm_id3_read(hm_id3_read_fn read, void *ctx, hm_id3 *out)
{
    unsigned char h[10], f[10], body[256];
    unsigned int end, pos, ver;

    memset(out, 0, sizeof(*out));
    if (read(ctx, 0, h, 10) != 10 || (end = hm_music_id3_size(h)) == 0)
        return -1;
    out->tag_len = end;
    ver = h[3];
    if (ver < 2 || ver > 4)
        return -1;
    end -= h[5] & 0x10 ? 10 : 0; /* (the footer) */
    pos = 10;
    if (h[5] & 0x40 && ver >= 3) { /* an extended header: skipped */
        unsigned char x[4];

        if (read(ctx, pos, x, 4) != 4)
            return 0;
        pos += ver == 4 ? syncsafe(x) : be32(x) + 4;
    }
    while (pos + (ver == 2 ? 6u : 10u) <= end) {
        unsigned int size, hl = ver == 2 ? 6u : 10u, n;
        char id[5];

        if (read(ctx, pos, f, hl) != (int)hl || f[0] == 0)
            break; /* the padding */
        if (ver == 2) {
            memcpy(id, f, 3);
            id[3] = 0;
            size = (unsigned int)f[3] << 16 | (unsigned int)f[4] << 8 | f[5];
        } else {
            memcpy(id, f, 4);
            id[4] = 0;
            size = ver == 4 ? syncsafe(f + 4) : be32(f + 4);
        }
        if (size == 0 || pos + hl + size > end)
            break;
        n = size < sizeof(body) ? size : (unsigned int)sizeof(body);
        if (!strcmp(id, "TIT2") || !strcmp(id, "TT2") || !strcmp(id, "TPE1") ||
            !strcmp(id, "TP1") || !strcmp(id, "TALB") || !strcmp(id, "TAL")) {
            char *dst = out->title;
            int cap = (int)sizeof(out->title);

            if (!strcmp(id, "TPE1") || !strcmp(id, "TP1")) {
                dst = out->artist;
                cap = (int)sizeof(out->artist);
            } else if (!strcmp(id, "TALB") || !strcmp(id, "TAL")) {
                dst = out->album;
                cap = (int)sizeof(out->album);
            }
            if (read(ctx, pos + hl, body, n) == (int)n)
                hm_id3_text(body, n, dst, cap);
        } else if ((!strcmp(id, "APIC") || !strcmp(id, "PIC")) && !out->pic_len) {
            /* encoding, the type (a MIME string; v2.2: three letters), the picture's kind, a
               description, then the picture */
            unsigned int k;
            int enc;

            if (read(ctx, pos + hl, body, n) != (int)n || n < 4)
                goto next;
            enc = body[0];
            k = 1;
            if (ver == 2) {
                k += 3;
            } else {
                while (k < n && body[k])
                    k++;
                k++;
            }
            k++; /* the picture type */
            if (enc == 1 || enc == 2) {
                while (k + 1 < n && (body[k] || body[k + 1]))
                    k += 2;
                k += 2;
            } else {
                while (k < n && body[k])
                    k++;
                k++;
            }
            if (k < n && k < size) {
                out->pic_off = pos + hl + k;
                out->pic_len = size - k;
            }
        }
    next:
        pos += hl + size;
    }
    return 0;
}

/* ---- MP3 frames (ISO/IEC 11172-3, 13818-3: the 4-byte header) ---- */

int hm_mp3_frame_len(unsigned int h)
{
    static const unsigned short rate1[16] = {0,   32,  40,  48,  56,  64,  80,  96,
                                             112, 128, 160, 192, 224, 256, 320, 0},
                                rate2[16] = {0,  8,  16, 24,  32,  40,  48,  56,
                                             64, 80, 96, 112, 128, 144, 160, 0},
                                freq[3] = {44100, 48000, 32000};
    int version = (int)(h >> 19) & 3, layer = (int)(h >> 17) & 3, bi = (int)(h >> 12) & 15,
        fi = (int)(h >> 10) & 3, pad = (int)(h >> 9) & 1, kbps, hz;

    if ((h & 0xFFE00000u) != 0xFFE00000u || version == 1 || layer != 1 || bi == 0 || bi == 15 ||
        fi == 3)
        return -1; /* not Layer III, or a free / bad rate */
    kbps = version == 3 ? rate1[bi] : rate2[bi];
    hz = freq[fi] >> (version == 3 ? 0 : version == 2 ? 1 : 2);
    return (version == 3 ? 144 : 72) * kbps * 1000 / hz + pad;
}

int hm_mp3_find_frame(const unsigned char *buf, int n, unsigned int ref)
{
    for (int i = 0; i + 4 <= n; i++) {
        unsigned int h, h2;
        int len;

        if (buf[i] != 0xFF || (buf[i + 1] & 0xE0) != 0xE0)
            continue;
        h = be32(buf + i);
        if ((h & 0xFFFE0C00u) != (ref & 0xFFFE0C00u) || (len = hm_mp3_frame_len(h)) <= 0)
            continue;
        if (i + len + 4 <= n) { /* the next frame must follow: no sync by chance */
            h2 = be32(buf + i + len);
            if ((h2 & 0xFFFE0C00u) != (ref & 0xFFFE0C00u) || hm_mp3_frame_len(h2) <= 0)
                continue;
        }
        return i;
    }
    return -1;
}

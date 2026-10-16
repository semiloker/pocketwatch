// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_RCO_H
#define PW_RCO_H

/*
 * Minimal reader for Sony's RCO resource files (flash0:/vsh/resource): finds an image by its label
 * and gives back where its (possibly compressed) data is. Structures as documented by RCOMage
 * (rcofile.h, LGPL-2.1, ZiNgA BuRgA); see docs/research/m4-native-icons.md. Portable,
 * bounds-checked.
 */

#define PW_RCO_NONE 0
#define PW_RCO_ZLIB 1
#define PW_RCO_RLZ  2

/* Inflates raw zlib/deflate data (the firmware's on the PSP). Returns bytes written or < 0. */
typedef int (*pw_rco_inflate_fn)(const void *src, unsigned int len, void *dst, unsigned int cap);

typedef struct {
    const unsigned char *file; /* the whole RCO */
    unsigned int size;
    const unsigned char *tables; /* the table area, unpacked; it stands in at file offset 0xA4 */
    unsigned int tables_len;
    unsigned int img_table, label_off, label_len, img_data;
} pw_rco;

typedef struct {
    int format;                            /* 5 = GIM */
    int compression;                       /* PW_RCO_* */
    unsigned int offset, packed, unpacked; /* offset in the file */
} pw_rco_image;

/*
 * Parse the header; unpack the tables into buf (cap bytes) when they are compressed. state is
 * PW_RLZ_STATE bytes of scratch. Returns 0 or < 0.
 */
int pw_rco_open(pw_rco *r, const void *file, unsigned int size, void *buf, unsigned int cap,
                unsigned char *state, pw_rco_inflate_fn inflate);

/* Find an image by label. Returns 0 or < 0. */
int pw_rco_find_image(const pw_rco *r, const char *label, pw_rco_image *img);

/* Unpack an image's data into dst. Returns its length or < 0. */
int pw_rco_read_image(const pw_rco *r, const pw_rco_image *img, void *dst, unsigned int cap,
                      unsigned char *state, pw_rco_inflate_fn inflate);

#endif

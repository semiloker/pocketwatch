// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_DISC_H
#define HM_DISC_H

/*
 * A game image on the card - ISO 9660, or CSO (blocks of raw deflate) - read far enough to find
 * the files of PSP_GAME/ (PARAM.SFO, ICON0.PNG, PIC1.PNG). Portable: the caller reads the file
 * and inflates (Pocketwatch Home, docs/subsystems/home.md).
 */

#define HM_SECTOR 2048

/* Read len bytes at offset of the file; returns the bytes read or < 0. */
typedef int (*hm_read_fn)(void *ctx, unsigned int offset, void *buf, unsigned int len);
/* Raw deflate (no zlib header) into out; returns the bytes produced or < 0. */
typedef int (*hm_inflate_fn)(void *out, int out_len, const void *in, int in_len);

typedef struct {
    hm_read_fn read;
    hm_inflate_fn inflate;
    void *ctx;
    int cso;
    unsigned int size; /* the image's size (uncompressed) */
    unsigned int block, align;
    /* one decompressed CSO block, and the compressed one */
    unsigned char out[HM_SECTOR], in[HM_SECTOR + 64];
    int cached; /* the block in out, or -1 */
} hm_disc;

/* 0, or < 0 when this is neither an ISO 9660 image nor a CSO. */
int hm_disc_open(hm_disc *d, hm_read_fn read, hm_inflate_fn inflate, void *ctx);

/* Read from the image as if it were uncompressed. Returns the bytes read or < 0. */
int hm_disc_read(hm_disc *d, unsigned int offset, void *buf, unsigned int len);

/* Find a file by its path ("PSP_GAME/PARAM.SFO", any case, no ";1"). 0 and offset/size, or < 0. */
int hm_disc_find(hm_disc *d, const char *path, unsigned int *offset, unsigned int *size);

/* An EBOOT.PBP (homebrew, PS1 games): its first HM_PBP_HEADER bytes name where each part is. */
#define HM_PBP_HEADER 40
#define HM_PBP_SFO    0 /* PARAM.SFO */
#define HM_PBP_ICON0  1
#define HM_PBP_PIC1   4

/* The offset and size of part in a PBP of file_size bytes: 0, or < 0 when this is not a PBP or
   its table does not fit the file. */
int hm_pbp_part(const unsigned char *hdr, unsigned int file_size, int part, unsigned int *offset,
                unsigned int *size);

#endif

// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_IMAGE_H
#define PW_IMAGE_H

/*
 * Images from the firmware's files: GIM (RCO icons) and 24-bit BMP (the XMB's month skies), both
 * decoded into 32-bit pixels in the PSP's byte order (R, G, B, A = 0xAABBGGRR), not premultiplied.
 * GIM as documented by Puyo Tools (MIT) and VitaXMB (GPL-3.0); see
 * docs/research/m4-native-icons.md. Portable, bounds-checked.
 */

typedef struct {
    unsigned int *px; /* w * h */
    int w, h;
} pw_image;

/* Decode a GIM into out (cap pixels). Returns 0, or < 0 (unsupported format, damaged, too big). */
int pw_gim_decode(const void *gim, unsigned int len, unsigned int *out, unsigned int cap,
                  pw_image *img);

/*
 * The index-th BMP of a file that holds several BMPs one after the other (01-12.bmp: twelve 60x34
 * month skies), 24-bit only. Returns 0 or < 0.
 */
int pw_bmp_decode(const void *file, unsigned int len, int index, unsigned int *out,
                  unsigned int cap, pw_image *img);

#endif

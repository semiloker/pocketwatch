// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_PHOTO_EDIT_H
#define HM_PHOTO_EDIT_H

/*
 * Home's photo editor (HOM-REQ-026, ADR-036): quarter turns, a mirror, a filter, brightness,
 * contrast, saturation, warmth and a vignette, applied to a preview the size of the screen while
 * the owner changes them, then to the picture itself when a copy is saved. Integer only, a table
 * per change (no division per pixel). Portable (tests/test_photo_edit.c).
 */

enum {
    HM_FL_NONE,
    HM_FL_MONO,
    HM_FL_SEPIA,
    HM_FL_VIVID,
    HM_FL_FADE,
    HM_FL_COOL,
    HM_FL_NOIR,
    HM_FL_RETRO,
    HM_FILTERS
};

/* The tools, in the order of the editor's bar. */
enum {
    HM_ED_ROTATE,
    HM_ED_MIRROR,
    HM_ED_FILTER,
    HM_ED_LIGHT,
    HM_ED_CONTRAST,
    HM_ED_SAT,
    HM_ED_WARMTH,
    HM_ED_VIGNETTE,
    HM_ED_SAVE,
    HM_ED_TOOLS
};

#define HM_ED_RANGE 5 /* the sliders go -5..5 (the vignette 0..5) */

typedef struct {
    int rotate; /* quarter turns clockwise, 0-3 */
    int mirror; /* 0 / 1 */
    int filter; /* HM_FL_* */
    int light, contrast, sat, warmth; /* -HM_ED_RANGE..HM_ED_RANGE */
    int vignette;                     /* 0..HM_ED_RANGE */
} hm_edit;

/* One step of tool t up (dir 1) or down (-1); 1 when something changed. */
int hm_edit_step(hm_edit *e, int tool, int dir);
/* Anything different from the picture as it is? */
int hm_edit_changed(const hm_edit *e);

/* The colours (filter, brightness, contrast, saturation, warmth) and the vignette, in place over
   w x h pixels (rows of stride). */
void hm_edit_colours(const hm_edit *e, unsigned int *px, int w, int h, int stride);

/* The turns and the mirror into a new picture (*ow x *oh, its own width as stride); the caller
   frees it with free(). NULL when there is no memory. */
unsigned int *hm_edit_turn(const unsigned int *px, int w, int h, int rotate, int mirror, int *ow,
                           int *oh);

/* The size of a picture w x h turned rotate quarter turns. */
void hm_edit_turned_size(int w, int h, int rotate, int *ow, int *oh);

#endif

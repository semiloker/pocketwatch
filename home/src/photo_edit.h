// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_PHOTO_EDIT_H
#define HM_PHOTO_EDIT_H

/*
 * Home's photo editor (HOM-REQ-040, ADR-060, #8), laid out as a professional editor: three tabs -
 * Adjust (exposure, contrast, highlights, shadows, saturation, vibrance, warmth, tint, vignette,
 * sharpen), Filters (a look and its strength) and Crop (quarter turns, a mirror, an aspect ratio) -
 * applied to the picture in the editor's canvas while the owner changes them, then to the picture
 * itself when a copy is saved. Integer only: tables per change, no division per pixel. Portable
 * (tests/test_photo_edit.c); main.c holds the pictures and the keys, home_view.c draws the
 * controls over hm_edit_compose's screen.
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

enum { HM_EDT_ADJUST, HM_EDT_FILTERS, HM_EDT_CROP, HM_EDT_TABS };

/* The Adjust tab's tools, in its strip's order. */
enum {
    HM_ADJ_EXPOSURE,
    HM_ADJ_CONTRAST,
    HM_ADJ_HIGHLIGHTS,
    HM_ADJ_SHADOWS,
    HM_ADJ_SATURATION,
    HM_ADJ_VIBRANCE,
    HM_ADJ_WARMTH,
    HM_ADJ_TINT,
    HM_ADJ_VIGNETTE,
    HM_ADJ_SHARPEN,
    HM_ADJS
};

/* The Crop tab's tools. */
enum { HM_CROP_ROTATE, HM_CROP_MIRROR, HM_CROP_ASPECT, HM_CROPS };

/* Aspect ratios, centred on the picture (in its orientation: 4:3 is 3:4 on a portrait one). */
enum { HM_AR_ORIGINAL, HM_AR_SQUARE, HM_AR_4_3, HM_AR_3_2, HM_AR_16_9, HM_ARS };

#define HM_ADJ_MAX  100 /* the sliders: -100..100; the vignette and sharpening 0..100 */
#define HM_ADJ_STEP 10  /* a press of up / down: a step one sees (#10) */
#define HM_STR_STEP 10  /* a filter's strength, 0..100 */

typedef struct {
    int adj[HM_ADJS];
    int filter, strength;       /* HM_FL_*, 0..100 */
    int rotate, mirror, aspect; /* quarter turns clockwise 0-3, 0 / 1, HM_AR_* */
} hm_edit;

/* The picture as it is (a filter's strength 100). */
void hm_edit_init(hm_edit *e);
/* The tools in a tab (Filters: one per look). */
int hm_edit_tools(int tab);
/* An Adjust tool that starts at 0 and only goes up (the vignette, sharpening). */
int hm_edit_one_sided(int adj);
/* Up (dir 1) or down (-1) on a tab's tool - in Filters, tool is the look (chosen if it was not,
   else its strength); 1 when something changed. */
int hm_edit_step(hm_edit *e, int tab, int tool, int dir);
/* Choose a look (Filters: left / right): 1 when it changed. */
int hm_edit_pick_filter(hm_edit *e, int filter);
/* A tool back to the picture as it is: 1 when something changed. */
int hm_edit_reset(hm_edit *e, int tab, int tool);
/* A tool changed from the picture as it is (its dot in the strip). */
int hm_edit_is_set(const hm_edit *e, int tab, int tool);
/* Anything different from the picture as it is? */
int hm_edit_changed(const hm_edit *e);

/* The colours (the look, the Adjust tab) and the vignette and sharpening, in place over w x h
   pixels (rows of stride). 0, or -1 when sharpening found no memory (the rest is done). */
int hm_edit_colours(const hm_edit *e, unsigned int *px, int w, int h, int stride);

/* The turns and the mirror into a new picture (*ow x *oh, its own width as stride); the caller
   frees it with free(). NULL when there is no memory. */
unsigned int *hm_edit_turn(const unsigned int *px, int w, int h, int rotate, int mirror, int *ow,
                           int *oh);
/* The size of a picture w x h turned rotate quarter turns. */
void hm_edit_turned_size(int w, int h, int rotate, int *ow, int *oh);
/* The part an aspect ratio keeps of a (turned) picture w x h: centred. */
void hm_edit_crop(int w, int h, int aspect, int *x, int *y, int *cw, int *ch);
/* Keep that part in place: px becomes cw x ch, its width as stride. */
void hm_edit_crop_in_place(unsigned int *px, int w, int x, int y, int cw, int ch);

/* A brightness histogram: HM_HIST_BINS bins, the fullest 255. */
#define HM_HIST_BINS 64
void hm_edit_histogram(const unsigned int *px, int w, int h, int stride,
                       unsigned char out[HM_HIST_BINS]);

/* ---- the editor's screen (480 x 272): what hm_edit_compose draws, what the view draws over ----
 */

#define HM_ED_SW     480
#define HM_ED_SH     272
#define HM_ED_TOP    26 /* the top bar: Cancel, the title, Save a Copy */
#define HM_ED_CX     0  /* the canvas the picture is fitted into */
#define HM_ED_CY     28
#define HM_ED_CW     480
#define HM_ED_CH     150
#define HM_ED_READ_Y 190 /* the chosen tool's name and value: their middle */
#define HM_ED_RULE_Y 207 /* the slider's track */
#define HM_ED_STRIP  233 /* the tools' (or the looks') row: its middle */
#define HM_ED_TABS_Y 261 /* the tabs: their middle */
#define HM_ED_PITCH  44  /* a tool's place in the strip */
#define HM_ED_TPITCH 54  /* a look's: its thumbnail and a gap */
#define HM_ED_THUMBW 46
#define HM_ED_THUMBH 30
#define HM_ED_DARK   0xFF141210u /* the editor's background (PW_RGB 16, 18, 20) */

/* The middle of item i of n in a strip of pitch, centred on the screen. */
static inline int hm_edit_strip_x(int n, int pitch, int i)
{
    return HM_ED_SW / 2 - n * pitch / 2 + i * pitch + pitch / 2;
}

/* The picture as the canvas shows it: src (w x h, fitted to the screen) turned, cropped and fitted
   into the canvas - what changes only with the Crop tab, so the caller keeps it between steps
   (#10: remaking it at each step was most of a step's 85 ms). *fw x *fh, its width as stride;
   free() it. NULL without memory. */
unsigned int *hm_edit_frame(const hm_edit *e, const unsigned int *src, int w, int h, int *fw,
                            int *fh);

/* The screen behind the editor's controls into bg (HM_ED_SW x HM_ED_SH, rows of 512): dark, frame
   (hm_edit_frame's, fw x fh) centred in the canvas with the edit's colours - as it is when
   original - and in the Filters tab each look on thumb (tw x th: HM_ED_THUMBW x HM_ED_THUMBH)
   along the strip. hist: the shown picture's histogram. 0, or -1 (no frame: bg dark). */
int hm_edit_compose(const hm_edit *e, const unsigned int *frame, int fw, int fh,
                    const unsigned int *thumb, int tw, int th, int tab, int original,
                    unsigned int *bg, unsigned char hist[HM_HIST_BINS]);

/* src (w x h) scaled into a new picture that fits fw x fh (its own width as stride): *ow x *oh;
   NULL without memory. Bilinear (for the canvas and the thumbnails, at most 2x down). */
unsigned int *hm_edit_fit(const unsigned int *src, int w, int h, int fw, int fh, int *ow, int *oh);

#endif

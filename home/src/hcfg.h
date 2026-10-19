// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_HCFG_H
#define HM_HCFG_H

/*
 * Pocketwatch Home's own settings (ADR-035): seplugins/pocketwatch/home.txt, "key = value"
 * lines, read and written only by Home - not config.txt, whose parser the XMB plugin shares at
 * its 64 KB budget (RISK-022). Unknown keys and bad values are ignored. Portable.
 */

enum { HM_BG_AUTO, HM_BG_SKY, HM_BG_GRADIENT, HM_BG_PLAIN, HM_BG_WALLPAPER, HM_BGS };
enum { HM_MOTION_NORMAL, HM_MOTION_FAST, HM_MOTION_CALM, HM_MOTIONS };
enum { HM_REPEAT_OFF, HM_REPEAT_ALL, HM_REPEAT_ONE };

#define HM_ACCENTS 9

typedef struct {
    int accent;  /* 0..HM_ACCENTS-1 (hm_cfg_accent) */
    int theme;   /* HM_THEME_* (home_view.h) */
    int bg;      /* HM_BG_*: the style's own (Classic the sky, else the gradient), ... */
    int sky;     /* 0 the XMB's theme colour (or the month), 1-12 that month's colour */
    int wave;    /* the XMB's wave over the sky (1) */
    int percent; /* the battery's percent beside its icon (1) */
    int repeat;  /* HM_REPEAT_* (all) */
    int shuffle; /* 0 */
    int motion;  /* HM_MOTION_*: how fast things glide */
} hm_cfg;

extern const char *const hm_cfg_accent_names[HM_ACCENTS];
extern const char *const hm_cfg_theme_names[4];
extern const char *const hm_cfg_bg_names[HM_BGS];
extern const char *const hm_cfg_motion_names[HM_MOTIONS];
extern const char *const hm_cfg_repeat_names[3];

void hm_cfg_defaults(hm_cfg *c);
int hm_cfg_parse(hm_cfg *c, const char *text, unsigned int len); /* settings taken */
int hm_cfg_format(const hm_cfg *c, char *out, unsigned int cap); /* length, or < 0 */

/* The accent colour (PW_RGB) of index i. */
unsigned int hm_cfg_accent(int i);
/* The time constant of the motion (ms) for a setting: PW_ANIM_TAU_MS scaled. */
int hm_cfg_tau(int motion, int tau_ms);

/* The settings in use; load reads path (none: defaults), save writes it. */
hm_cfg *hm_cfg_get(void);
int hm_cfg_load(const char *path);
int hm_cfg_save(const char *path);

#endif

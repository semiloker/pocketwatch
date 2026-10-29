// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_PERF_OVERLAY_H
#define PW_PERF_OVERLAY_H

/*
 * The game overlay's text and drawing (PRF-REQ-001..005, ADR-043): what the module measured as one
 * or two short lines on a black box, written straight into the frame the game shows - the plugin's
 * 8x8 font with a black outline, and no read of VRAM (uncached reads are slow).
 * Portable (no PSP headers, host-tested): perf/src/main.c measures and finds the frame.
 */

enum { PW_PERF_OFF, PW_PERF_FPS, PW_PERF_FULL };

#define PW_PERF_NA   (-32768) /* a value not known */
#define PW_PERF_COLS 18       /* characters on a row of the card (short labels) */
#define PW_PERF_ROWS 6        /* rows of the card at most */

typedef struct {
    int fps;              /* frames the game showed in the last second */
    int cpu_pct;          /* the CPU's busy share: 100 less the idle threads' */
    int cpu_mhz, bus_mhz; /* the clocks */
    int free_kb;          /* user memory free */
    int bat_pct, bat_ma, bat_mv, bat_temp, bat_min; /* the battery: %, mA, mV, C, minutes left */
} pw_perf_stats;

/* The card's rows for mode (FPS: one; Full: a short labelled row each, like Home's overlay -
   "60 FPS", "CPU 43%", "333 MHz", ...): how many. Unknown values are left out. */
int pw_perf_rows(const pw_perf_stats *s, int mode, char rows[PW_PERF_ROWS][PW_PERF_COLS + 1]);

/* A line's pixels, made once a second (pw_perf_render) and written into every frame
   (pw_perf_draw): its letters and their black outline, a bit each - finding the outline is what
   costs, not the writes. */
#define PW_PERF_W (PW_PERF_COLS * 8 + 2) /* px: the letters and a pixel each side */
#define PW_PERF_H 10
typedef struct {
    unsigned char lit[PW_PERF_H][(PW_PERF_W + 7) / 8], edge[PW_PERF_H][(PW_PERF_W + 7) / 8];
    int w; /* px used */
} pw_perf_mask;

void pw_perf_render(const char *text, pw_perf_mask *m);

/* m at x, y (its top left) into a frame: fb its first pixel, stride in pixels, fmt the display's
   pixel format (0 5650, 1 5551, 2 4444, 3 8888), clipped to w x h. */
void pw_perf_draw(void *fb, int stride, int fmt, int w, int h, int x, int y, const pw_perf_mask *m);
/* A black box under them (bw x bh at x, y), clipped. */
void pw_perf_box(void *fb, int stride, int fmt, int w, int h, int x, int y, int bw, int bh);

/* The next mode when L + R + SELECT were held a second: off, the frame rate, everything, off. */
int pw_perf_next(int mode);

/* perf.txt (Home writes it, the module reads it at a game's start): "mode = off|fps|full" (the
   numbers), "osd = off|home|all" (the volume and brightness indicator: none, over Home only, over
   games too), "boost = <0-100>" (the backlight: that share of the way from the system's level up
   to the most, ADR-045), "steps = on|off" and "levels = 20 30 ..." (Pocketwatch's own brightness
   steps for the SCREEN button instead of the system's PW_BRIGHT_STEPS, ADR-047: up to
   PW_STEPS_MAX levels of 1-100). What is missing or not known: full, home, 0, off,
   PW_STEPS_DEFAULT. */
enum { PW_OSD_OFF, PW_OSD_HOME, PW_OSD_ALL };
/* The system's own steps for SCREEN: impose's brightness 0-4 (#6). ARK-4 makes
   scePowerGetBacklightMaximum always 4 (core/compat/psp/syspatch.c, patch_scePower_Service): the
   fifth step, which the firmware keeps for the AC adapter, on battery too. */
#define PW_BRIGHT_STEPS  5
#define PW_STEPS_MAX     10
#define PW_STEPS_DEFAULT {20, 30, 40, 50, 60, 70, 85, 100}
typedef struct {
    int mode, osd, boost;
    int steps;                         /* Pocketwatch's steps instead of the system's */
    int levels[PW_STEPS_MAX], nlevels; /* the steps' backlight levels, darkest first */
} pw_perf_cfg;
void pw_perf_cfg_parse(const char *text, int len, pw_perf_cfg *c);

/* ---- the brightness boost (ADR-045): portable, the module's and Home's ---- */

/* The level boost gives for the system's level (1-100); 0 (off) stays 0. */
int pw_bright_boost(int level, int boost);

/* Keeping it while the system changes the level (the SCREEN button, the charger, a wake): given
   the display's level now, the one to set, or -1 to leave it - the boosted system level, or fixed
   (Pocketwatch's step) when fixed > 0. A boost turned off gives the system's level back; the
   screen off stays off. When another (a brightness plugin) sets its own again and again, within
   PW_BRIGHT_FIGHT_MS of ours, PW_BRIGHT_FIGHTS times, it gives up (fights >= PW_BRIGHT_FIGHTS);
   expected: the change is the owner's SCREEN press (no fight). */
#define PW_BRIGHT_FIGHTS   4
#define PW_BRIGHT_FIGHT_MS 400
typedef struct {
    int set;    /* the level set here last, -1 none */
    int base;   /* the system's level under it */
    int fights; /* times it was put back at once */
    unsigned int t_set;
} pw_bright;
void pw_bright_init(pw_bright *b);
int pw_bright_step(pw_bright *b, int level, int boost, int fixed, int expected,
                   unsigned int now_ms);

/* Pocketwatch's steps (ADR-047): a short press of SCREEN (released within PW_STEPS_TAP_MS) is the
   next step, round from the brightest to the darkest; a long one is the system's (the screen
   off). */
#define PW_STEPS_TAP_MS 600
typedef struct {
    int cur;  /* the step now */
    int down; /* SCREEN held */
    unsigned int t_down;
} pw_steps;
/* The step whose level is nearest to level. */
int pw_steps_nearest(const pw_perf_cfg *c, int level);
/* SCREEN now (held 1 / 0): 1 when the step changed. */
int pw_steps_button(pw_steps *s, int held, int n, unsigned int now_ms);

/* ---- the volume and brightness indicator ---- */

enum { PW_OSD_NONE, PW_OSD_VOLUME, PW_OSD_BRIGHT };
#define PW_OSD_MS 1500 /* shown this long after the last change */
typedef struct {
    int vol, mute, bright; /* as last read (-1 not known) */
    int kind;              /* PW_OSD_*: what changed last */
    unsigned int t;        /* when */
    int primed;            /* (the first reading only remembered) */
} pw_osd;
/* The readings now (-1: not known): 1 when one changed (o->kind, o->t set). */
int pw_osd_step(pw_osd *o, int vol, int mute, int bright, unsigned int now_ms);
/* What to show now: the kind within PW_OSD_MS of its change, else PW_OSD_NONE. */
int pw_osd_shown(const pw_osd *o, unsigned int now_ms);
/* Its words for games (the 8x8 font has no other letters): "VOL 18", "MUTE", "BRIGHT 3/5 92%" -
   o->bright of steps (the system's PW_BRIGHT_STEPS, or Pocketwatch's). */
void pw_osd_text(const pw_osd *o, int level, int steps, char out[PW_PERF_COLS + 1]);

/* ---- the game's display framebuffers ----

   The game cycles through two or three buffers (double/triple buffering) and shows them in turn;
   writing the overlay into only one makes it appear every other frame - the flicker the owner saw.
   The module remembers the buffers it has shown and draws into all of them, so whichever is on
   screen carries the overlay. */
#define PW_FB_MAX 4
typedef struct {
    unsigned int fb[PW_FB_MAX];
    int w[PW_FB_MAX], f[PW_FB_MAX]; /* each one's line width (pixels) and pixel format */
    int n;                          /* distinct buffers kept */
    int next;                       /* where the next new one goes (the oldest is dropped) */
} pw_fbset;
/* Remember a shown framebuffer; an address already kept has its width/format refreshed. */
void pw_fbset_add(pw_fbset *s, unsigned int fb, int w, int f);

/* The indicator in a game's frame: a black box with a white edge at the bottom middle, label in
   it and a bar of n steps, k of them lit (format, clip: pw_perf_draw's). */
#define PW_OSD_W 280
#define PW_OSD_H 24
void pw_osd_draw(void *fb, int stride, int fmt, int w, int h, const pw_perf_mask *label, int k,
                 int n);

#endif

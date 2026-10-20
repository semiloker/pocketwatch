// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_PERF_OVERLAY_H
#define PW_PERF_OVERLAY_H

/*
 * The game overlay's text and drawing (PRF-REQ-001..005, ADR-043): what the module measured as one
 * or two lines, written straight into the frame the game shows - the plugin's 8x8 font with a
 * black outline, so it reads on any picture, and no read of VRAM (uncached reads are slow).
 * Portable (no PSP headers, host-tested): perf/src/main.c measures and finds the frame.
 */

enum { PW_PERF_OFF, PW_PERF_FPS, PW_PERF_FULL };

#define PW_PERF_NA   (-32768) /* a value not known */
#define PW_PERF_COLS 59       /* characters on a line: 472 of the screen's 480 px */

typedef struct {
    int fps;              /* frames the game showed in the last second */
    int cpu_pct;          /* the CPU's busy share: 100 less the idle threads' */
    int cpu_mhz, bus_mhz; /* the clocks */
    int free_kb;          /* user memory free */
    int bat_pct, bat_ma, bat_mv, bat_temp, bat_min; /* the battery: %, mA, mV, C, minutes left */
} pw_perf_stats;

/* The lines for mode (PW_PERF_FPS: one, PW_PERF_FULL: two): how many. Unknown values are left
   out. */
int pw_perf_lines(const pw_perf_stats *s, int mode, char lines[2][PW_PERF_COLS + 1]);

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

/* The next mode when L + R + SELECT were held a second: off, the frame rate, everything, off. */
int pw_perf_next(int mode);

/* perf.txt (Home writes it): "mode = fps" or "mode = full"; anything else: full. */
int pw_perf_parse(const char *text, int len);

#endif

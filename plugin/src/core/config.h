// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_CONFIG_H
#define PW_CONFIG_H

/*
 * The owner's settings (SYS-REQ-010): seplugins/pocketwatch/config.txt, "key = value" lines,
 * written by the hub's Settings screen. Unknown keys and bad values are ignored, so an old or
 * hand-edited file never stops the plugin. The language is not here: it is language.txt
 * (ADR-018). Portable.
 */

typedef struct {
    int fast;   /* touch-and-go suspends after the first save of a session (default 1) */
    int status; /* clock and battery in the hub's corner (default 1) */
    int style;  /* the menu's look: 0 XMB (default), 1 modern, 2 minimal (ui/ui.h PW_STYLE_*) */
    int start; /* the screen the PSP starts in: 0 the XMB (default), 1 Pocketwatch Home (ADR-026) */
    int sort;  /* Home's games: 0 by name (default), 1 the last played first */
    int slide; /* Home's slideshow: 0 normal (5 s a picture, default), 1 fast (3 s), 2 slow (10 s)
                */
} pw_config;

/* The names config.txt uses for the styles and the start screens, by index. */
extern const char *const pw_config_style_names[3];
extern const char *const pw_config_start_names[2];
extern const char *const pw_config_sort_names[2];
extern const char *const pw_config_slide_names[3];

void pw_config_defaults(pw_config *c);

/* Apply the lines of text (len bytes) over c. Returns how many settings were taken. */
int pw_config_parse(pw_config *c, const char *text, unsigned int len);

/* Write c as text. Returns the length, or < 0 if cap is too small. */
int pw_config_format(const pw_config *c, char *out, unsigned int cap);

/* The settings in use; pw_config_load reads them from path (missing file: defaults). */
pw_config *pw_config_get(void);
int pw_config_load(const char *path);
int pw_config_save(const char *path);

#endif

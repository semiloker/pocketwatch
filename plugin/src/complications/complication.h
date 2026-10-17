// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_COMPLICATION_H
#define PW_COMPLICATION_H

/*
 * A complication (ADR-009, docs/subsystems/complications.md) is one folder under complications/
 * plus one line in complications/table.c. It puts rows into the hub, runs when one of them is
 * chosen and has optional lifecycle hooks; the hub knows nothing else about it (CPL-REQ-001..004).
 */

#include "core/freeze.h"
#include "ui/ui.h"

#define PW_CPL_DESC_MAX 80
#define PW_CPL_ROWS_MAX 4 /* rows one complication may add */

/* One row of the hub. */
typedef struct {
    int art, glow; /* PW_ART_* (gfx/assets.h), or -1 */
    int fallback;  /* PW_ICON_* (ui/icons.h) drawn when the art is missing */
    const char *title;
    char desc[PW_CPL_DESC_MAX]; /* "" for none */
    int enabled;                /* shown dimmer when 0; run() still gets Enter to explain why */
} pw_cpl_item;

/* What the hub hands a complication when one of its rows is chosen. */
typedef struct {
    pw_ui *ui;
    const pw_freeze_set *frozen; /* the game, frozen while the hub is open */
} pw_cpl_env;

/* What the hub does after run(). */
enum {
    PW_CPL_STAY,   /* rebuild the rows, stay open */
    PW_CPL_CLOSE,  /* close the hub (Resume Game) */
    PW_CPL_QUIT,   /* close and quit the game */
    PW_CPL_LOADED, /* a state was loaded: the hub's screen is gone, close at once */
};

typedef struct {
    const char *id;
    void (*init)(void);                           /* plugin start, may do I/O; NULL: none */
    void (*shutdown)(void);                       /* module_stop, no I/O; NULL: none */
    void (*on_open)(void);                        /* the hub opens; NULL: none */
    int (*items)(pw_cpl_item *out, int max);      /* its rows, in order; returns how many */
    int (*run)(int index, const pw_cpl_env *env); /* Enter on its row `index`: PW_CPL_* */
    int (*changed)(void);                         /* asked every second: 1 = rebuild; NULL: never */
} pw_complication;

#endif

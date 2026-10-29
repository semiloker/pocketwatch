// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspkernel.h>
#include <psputility.h>
#include <string.h>

#include "dialogs.h"
#include "gfx_gu.h"

/* The threads' priorities every PSPSDK utility sample uses. */
void hm_dialog_base(void *common, unsigned int size, int cross_enter, int language)
{
    pspUtilityDialogCommon *b = common;

    memset(b, 0, size);
    b->size = size;
    b->language = language;
    b->buttonSwap = cross_enter ? PSP_UTILITY_ACCEPT_CROSS : PSP_UTILITY_ACCEPT_CIRCLE;
    b->graphicsThread = 17;
    b->accessThread = 19;
    b->fontThread = 18;
    b->soundThread = 16;
}

static void sd_update(void)
{
    sceUtilitySavedataUpdate(1);
}
static void sd_shutdown(void)
{
    sceUtilitySavedataShutdownStart();
}

/* One frame with the dialog over Home: our list, the GE done, then the dialog's own drawing. */
void hm_dialog_run(hm_status_fn status, hm_update_fn update, hm_update_fn shutdown,
                   hm_frame_fn frame, void *ctx)
{
    for (;;) {
        int st = status();

        frame(ctx);
        hm_gu_sync();
        if (st == PSP_UTILITY_DIALOG_VISIBLE)
            update();
        else if (st == PSP_UTILITY_DIALOG_QUIT)
            shutdown();
        hm_gu_present();
        if (st < 0 || st == PSP_UTILITY_DIALOG_NONE || st == PSP_UTILITY_DIALOG_FINISHED)
            break; /* (an error: no dialog to wait for) */
    }
}

int hm_dialog_savedata(const char *disc_id, int cross_enter, int language, hm_frame_fn frame,
                       void *ctx)
{
    /* "<>": every folder of the game (gameName + anything), as the firmware lists them */
    static char names[2][20] = {"<>", ""};
    static SceUtilitySavedataParam p;

    memset(&p, 0, sizeof(p));
    hm_dialog_base(&p.base, sizeof(p), cross_enter, language);
    p.mode = PSP_UTILITY_SAVEDATA_LISTDELETE;
    strncpy(p.gameName, disc_id, sizeof(p.gameName) - 1);
    p.saveNameList = names;
    if (sceUtilitySavedataInitStart(&p) < 0)
        return -1;
    hm_dialog_run(sceUtilitySavedataGetStatus, sd_update, sd_shutdown, frame, ctx);
    return 0;
}

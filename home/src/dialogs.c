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

static void osk_update(void)
{
    sceUtilityOskUpdate(1);
}
static void osk_shutdown(void)
{
    sceUtilityOskShutdownStart();
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

/* ---- UTF-8 <-> UTF-16 (the keyboard's text) ---- */

static int to16(const char *s, unsigned short *out, int cap)
{
    int n = 0;

    while (*s && n < cap - 1) {
        unsigned int c = (unsigned char)*s++;

        if (c >= 0xE0 && s[0] && s[1]) {
            c = (c & 0x0F) << 12 | ((unsigned char)s[0] & 0x3F) << 6 | ((unsigned char)s[1] & 0x3F);
            s += 2;
        } else if (c >= 0xC0 && s[0]) {
            c = (c & 0x1F) << 6 | ((unsigned char)s[0] & 0x3F);
            s += 1;
        }
        out[n++] = (unsigned short)c;
    }
    out[n] = 0;
    return n;
}

static void to8(const unsigned short *s, char *out, int cap)
{
    int n = 0;

    for (; *s && n < cap - 4; s++) {
        unsigned int c = *s;

        if (c < 0x80) {
            out[n++] = (char)c;
        } else if (c < 0x800) {
            out[n++] = (char)(0xC0 | c >> 6);
            out[n++] = (char)(0x80 | (c & 0x3F));
        } else {
            out[n++] = (char)(0xE0 | c >> 12);
            out[n++] = (char)(0x80 | ((c >> 6) & 0x3F));
            out[n++] = (char)(0x80 | (c & 0x3F));
        }
    }
    out[n] = 0;
}

int hm_dialog_osk(const char *title, const char *initial, char *out, int cap, int cross_enter,
                  int language, hm_frame_fn frame, void *ctx)
{
    static unsigned short desc[64], in[64], text[64];
    SceUtilityOskData data;
    SceUtilityOskParams p;

    to16(title, desc, 64);
    to16(initial, in, 64);
    memset(text, 0, sizeof(text));
    memset(&data, 0, sizeof(data));
    data.language = PSP_UTILITY_OSK_LANGUAGE_DEFAULT;
    data.lines = 1;
    data.unk_24 = 1;
    data.inputtype = PSP_UTILITY_OSK_INPUTTYPE_ALL;
    data.desc = desc;
    data.intext = in;
    data.outtextlength = 64;
    data.outtextlimit = 32; /* a nickname's length in the XMB */
    data.outtext = text;
    hm_dialog_base(&p.base, sizeof(p), cross_enter, language);
    p.datacount = 1;
    p.data = &data;
    if (sceUtilityOskInitStart(&p) < 0)
        return -1;
    hm_dialog_run(sceUtilityOskGetStatus, osk_update, osk_shutdown, frame, ctx);
    if (data.result != PSP_UTILITY_OSK_RESULT_CHANGED)
        return 0;
    to8(text, out, cap);
    return 1;
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

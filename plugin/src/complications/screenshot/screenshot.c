// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * The Screenshot complication (CPL-REQ-010, ADR-037): the game's picture as it was when the hub
 * opened - the frame the hub captures for its backdrop - as a PNG in ms0:/PICTURE/<disc ID>/,
 * the folder the XMB's Photo shows and Pocketwatch Home lists as the game's album under the
 * game's name. The game is frozen meanwhile and the card answers (ADR-032): the write is the
 * same kind as a state's.
 */
#include <psprtc.h>

#include "complications/complication.h"
#include "core/game.h"
#include "core/log.h"
#include "core/mem.h"
#include "core/platform.h"
#include "core/png.h"
#include "gfx/assets.h"
#include "stb_sprintf.h"
#include "ui/dialog.h"
#include "ui/fb.h"
#include "ui/i18n.h"
#include "ui/icons.h"

static void row(void *ctx, unsigned int y, unsigned char *rgb)
{
    const unsigned int *p = (const unsigned int *)ctx + y * PW_FB_STRIDE;

    for (int x = 0; x < PW_FB_W; x++, rgb += 3) {
        rgb[0] = (unsigned char)p[x];
        rgb[1] = (unsigned char)(p[x] >> 8);
        rgb[2] = (unsigned char)(p[x] >> 16);
    }
}

static int items(pw_cpl_item *out, int max)
{
    if (max < 1)
        return 0;
    out[0].art = PW_ART_PHOTO;
    out[0].glow = -1;
    out[0].fallback = PW_ICON_CARD;
    out[0].title = pw_tr(PW_S_SCREENSHOT);
    out[0].enabled = pw_fb_game() != 0;
    return 1;
}

static int run(int index, const pw_cpl_env *env)
{
    const unsigned int *px = pw_fb_game();
    const pw_game *g = pw_game_get();
    const char *id = g->ok && g->id[0] ? g->id : "Pocketwatch";
    unsigned int cap = pw_png_size(PW_FB_W, PW_FB_H);
    char path[96] = "", shown[80];
    pw_mem_block mem;
    ScePspDateTime t;
    int len, ok = 0;

    (void)index;
    if (!px || pw_log_card_blocked())
        return PW_CPL_STAY;
    pw_dialog_busy(env->ui, pw_tr(PW_S_SAVING));
    if (pw_mem_alloc(&mem, "PwShot", cap + 1 + 3 * PW_FB_W + 64) == 0) {
        len = pw_png_encode_rgb(PW_FB_W, PW_FB_H, row, (void *)px,
                                (unsigned char *)mem.ptr + cap, mem.ptr, cap);
        pw_dir_make("ms0:/PICTURE");
        stbsp_snprintf(path, sizeof(path), "ms0:/PICTURE/%s", id);
        pw_dir_make(path);
        if (sceRtcGetCurrentClockLocalTime(&t) < 0)
            t.year = t.month = t.day = t.hour = t.minute = t.second = 0;
        stbsp_snprintf(path, sizeof(path), "ms0:/PICTURE/%s/%s_%04d%02d%02d_%02d%02d%02d.png", id,
                       id, t.year, t.month, t.day, t.hour, t.minute, t.second);
        ok = len > 0 && pw_file_write(path, mem.ptr, (unsigned int)len, 0) == len;
        pw_mem_free(&mem);
    }
    pw_log("screenshot: %s %s", path, ok ? "written" : "FAILED");
    stbsp_snprintf(shown, sizeof(shown), pw_tr(PW_S_SHOT_SAVED), id);
    pw_dialog_message(env->ui, ok ? shown : pw_tr(PW_S_WRITE_FAILED), 0);
    return PW_CPL_STAY;
}

const pw_complication pw_cpl_screenshot = {.id = "screenshot", .items = items, .run = run};

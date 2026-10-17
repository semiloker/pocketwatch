// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * The System complication (CPL-REQ-005): Resume Game first in the hub, Quit Game last. Two table
 * entries, so the other complications' rows sit between them.
 */
#include "complications/complication.h"
#include "gfx/assets.h"
#include "ui/dialog.h"
#include "ui/i18n.h"
#include "ui/icons.h"

static int resume_items(pw_cpl_item *out, int max)
{
    if (max < 1)
        return 0;
    out[0].art = PW_ART_UMD;
    out[0].glow = PW_ART_UMD_GLOW;
    out[0].fallback = PW_ICON_RESUME;
    out[0].title = pw_tr(PW_S_RESUME_GAME);
    out[0].enabled = 1;
    return 1;
}

static int resume_run(int index, const pw_cpl_env *env)
{
    (void)index;
    (void)env;
    return PW_CPL_CLOSE;
}

static int quit_items(pw_cpl_item *out, int max)
{
    if (max < 1)
        return 0;
    out[0].art = -1; /* the XMB has no such icon: the drawn power symbol */
    out[0].glow = -1;
    out[0].fallback = PW_ICON_QUIT;
    out[0].title = pw_tr(PW_S_QUIT_GAME);
    out[0].enabled = 1;
    return 1;
}

static int quit_run(int index, const pw_cpl_env *env)
{
    (void)index;
    return pw_dialog_confirm(env->ui, pw_tr(PW_S_CONFIRM_QUIT), 0, 0) ? PW_CPL_QUIT : PW_CPL_STAY;
}

const pw_complication pw_cpl_resume = {"resume", 0, 0, 0, resume_items, resume_run, 0};
const pw_complication pw_cpl_quit = {"quit", 0, 0, 0, quit_items, quit_run, 0};

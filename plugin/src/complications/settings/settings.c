// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Settings in the game (CPL-REQ-006): config.txt is read at plugin start and the engine's fast
 * mode applied. The settings themselves are changed in the XMB (Settings -> Pocketwatch,
 * vsh/; ADR-024): the hub has no Settings row any more.
 */
#include <string.h>

#include "complications/complication.h"
#include "complications/states/engine.h"
#include "core/config.h"
#include "core/log.h"
#include "stb_sprintf.h"

static void config_path(char *out, unsigned int cap)
{
    char dir[200];

    if (pw_log_dir(dir, sizeof(dir)) < 0)
        strcpy(dir, "ms0:/seplugins/pocketwatch/");
    stbsp_snprintf(out, (int)cap, "%sconfig.txt", dir);
}

static void settings_init(void)
{
    char path[256];

    config_path(path, sizeof(path));
    pw_config_load(path);
    pw_engine_set_fast(pw_config_get()->fast);
}

const pw_complication pw_cpl_settings = {.id = "settings", .init = settings_init};

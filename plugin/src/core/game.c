// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspinit.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <string.h>

#include "complications/states/sfo.h"
#include "core/game.h"
#include "core/log.h"
#include "core/platform.h"

/* ARK-4 SystemCtrlForKernel (core/stubs/SystemCtrlForKernel.S): the mounted ISO's path. */
char *sctrlSEGetUmdFile(void);

#define SFO_MAX 4096

static pw_game g_game;

/* Retail disc/ISO apitypes (ARK-4 runlevel "umd"); disc0: is only mounted for these. */
static int is_disc_apitype(int apitype)
{
    return apitype == 0x120 || (apitype >= 0x123 && apitype <= 0x126) || apitype == 0x130 ||
           apitype == 0x160 || (apitype >= 0x110 && apitype <= 0x115);
}

static void read_id(void)
{
    char buf[32];
    int n = pw_file_read("disc0:/UMD_DATA.BIN", buf, sizeof(buf) - 1), j = 0;

    if (n <= 0)
        return;
    buf[n] = 0;
    for (int i = 0; i < n && buf[i] != '|' && j < (int)sizeof(g_game.id) - 1; i++)
        if (buf[i] != '-')
            g_game.id[j++] = buf[i];
    g_game.id[j] = 0;
}

static void read_sfo(void)
{
    static unsigned char sfo[SFO_MAX];
    int n = pw_file_read("disc0:/PSP_GAME/PARAM.SFO", sfo, sizeof(sfo));
    unsigned int off, len;

    if (n <= 0)
        return;
    if (pw_sfo_find(sfo, (unsigned int)n, "TITLE", &off, &len) == 0) {
        if (len >= sizeof(g_game.title))
            len = sizeof(g_game.title) - 1;
        memcpy(g_game.title, sfo + off, len);
        g_game.title[len] = 0;
    }
    if (pw_sfo_find(sfo, (unsigned int)n, "PARENTAL_LEVEL", &off, &len) == 0 && len >= 4)
        g_game.parental = sfo[off] | (sfo[off + 1] << 8);
}

void pw_game_init(void)
{
    const char *iso;

    memset(&g_game, 0, sizeof(g_game));
    g_game.parental = 1;
    if (!is_disc_apitype(sceKernelInitApitype()))
        return;
    read_id();
    read_sfo();
    iso = sctrlSEGetUmdFile();
    if (iso && iso[0]) {
        strncpy(g_game.iso, iso, sizeof(g_game.iso) - 1);
        int size = pw_file_size(iso);

        g_game.iso_size = size > 0 ? (unsigned int)size : 0;
    }
    g_game.ok = g_game.id[0] != 0;
    if (g_game.title[0] == 0)
        strcpy(g_game.title, g_game.id);
    pw_log("game: %s \"%s\" from %s (%u KB)", g_game.ok ? g_game.id : "(unknown)", g_game.title,
           g_game.iso[0] ? g_game.iso : "UMD", g_game.iso_size / 1024);
}

const pw_game *pw_game_get(void)
{
    return &g_game;
}

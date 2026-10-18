// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspkernel.h>
#include <string.h>

#include "launch.h"

#define ISO_RUNLEVEL   0x123 /* an image on ms0 (arkMenu include/entry.h) */
#define APP_RUNLEVEL   0x141 /* homebrew on ms0 */
#define PS1_RUNLEVEL   0x144 /* a PS1 game (POPS) on ms0 */
#define BOOT_UMD       0     /* sctrlSESetBootConfFileIndex: MODE_UMD, no image */
#define BOOT_INFERNO   3     /* sctrlSESetBootConfFileIndex: MODE_INFERNO */
#define DISC_TYPE_GAME 0x10  /* PSP_UMD_TYPE_GAME */
#define EBOOT_BIN      "disc0:/PSP_GAME/SYSDIR/EBOOT.BIN"
#define EBOOT_OLD      "disc0:/PSP_GAME/SYSDIR/EBOOT.OLD" /* a patched image keeps the original */

/* PSPSDK's SceKernelLoadExecVSHParam (psploadexec_kernel.h), without its kernel header. */
typedef struct {
    SceSize size, args;
    void *argp;
    const char *key;
    unsigned int vshmain_args_size;
    void *vshmain_args;
    char *configfile;
    unsigned int unk4, unk5;
} vsh_param;

/* src/stubs.S: ARK-4's SystemCtrlForUser */
int sctrlSESetDiscType(int type);
int sctrlSESetBootConfFileIndex(int index);
void sctrlSESetUmdFile(const char *file);
int sctrlKernelLoadExecVSHWithApitype(int apitype, const char *file, vsh_param *param);

int hm_launch(const char *image, int patched)
{
    static vsh_param p;
    const char *eboot = patched ? EBOOT_OLD : EBOOT_BIN;

    memset(&p, 0, sizeof(p));
    p.size = sizeof(p);
    p.key = "umdemu";
    p.argp = (void *)eboot;
    p.args = strlen(eboot) + 1;
    sctrlSESetDiscType(DISC_TYPE_GAME);
    sctrlSESetBootConfFileIndex(BOOT_INFERNO);
    sctrlSESetUmdFile(image);
    return sctrlKernelLoadExecVSHWithApitype(ISO_RUNLEVEL, image, &p);
}

int hm_launch_eboot(const char *eboot, int ps1)
{
    static vsh_param p;

    memset(&p, 0, sizeof(p));
    p.size = sizeof(p);
    p.key = ps1 ? "pops" : "game";
    p.argp = (void *)eboot;
    p.args = strlen(eboot) + 1;
    sctrlSESetUmdFile("");
    sctrlSESetBootConfFileIndex(BOOT_UMD);
    return sctrlKernelLoadExecVSHWithApitype(ps1 ? PS1_RUNLEVEL : APP_RUNLEVEL, eboot, &p);
}

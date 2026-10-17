// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Pocketwatch's XMB plugin (VSH runlevel, user mode): the "Pocketwatch" entry under Settings
 * (VSH-REQ-006, docs/subsystems/vsh-integration.md). It only patches the XMB's modules as they
 * start; everything else happens in the XMB's own threads.
 */
#include <pspkernel.h>
#include <psploadcore.h>
#include <string.h>

#include "xlog.h"
#include "xmb.h"

PSP_MODULE_INFO("PocketwatchXMB", 0x0007, 1, 0);

typedef int (*start_handler)(SceModule *mod);

start_handler sctrlHENSetStartModuleHandler(start_handler handler); /* src/stubs.S */

static start_handler g_previous;

/* The patch lines are written once the XMB is up (no file I/O in the module manager's thread);
   later lines are written by the XMB's own threads as things happen. */
static int flush_later(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    sceKernelDelayThread(15 * 1000 * 1000);
    pwx_log_flush();
    sceKernelExitDeleteThread(0);
    return 0;
}

/*
 * ARK-4 calls the newest handler as each module starts, before its code runs; each handler calls
 * the one before it. Ours finds its sites first, lets the others (xmbctrl) patch, then wraps.
 */
static int on_module_start(SceModule *mod)
{
    int vsh = strcmp(mod->modname, "vsh_module") == 0,
        sysconf = strcmp(mod->modname, "sysconf_plugin_module") == 0, ret = 0;

    if (vsh)
        pw_xmb_vsh_before(mod->text_addr, mod->text_size);
    else if (sysconf)
        pw_xmb_sysconf_before(mod->text_addr, mod->text_size);
    if (g_previous)
        ret = g_previous(mod);
    if (vsh)
        pw_xmb_vsh_after();
    else if (sysconf)
        pw_xmb_sysconf_after();
    return ret;
}

int module_start(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    pwx_log("Pocketwatch XMB " PW_VERSION);
    g_previous = sctrlHENSetStartModuleHandler(on_module_start);
    {
        SceUID th = sceKernelCreateThread("PocketwatchXmbLog", flush_later, 0x30, 0x1000, 0, 0);

        if (th >= 0)
            sceKernelStartThread(th, 0, 0);
    }
    return 0;
}

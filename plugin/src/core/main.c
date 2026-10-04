// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Pocketwatch - Pause. Save. Play.
 * Kernel-mode PRX entry point, loaded by the CFW in the GAME runlevel.
 */
#include <pspkernel.h>
#include <string.h>

#include "core/log.h"

PSP_MODULE_INFO("Pocketwatch", PSP_MODULE_KERNEL, 0, 1);

#ifndef PW_VERSION
#define PW_VERSION "dev"
#endif

#define PW_DEFAULT_PATH     "ms0:/seplugins/pocketwatch/pocketwatch.prx"
#define PW_THREAD_PRIORITY  0x20
#define PW_THREAD_STACK     0x1000

static char g_module_path[256];

/*
 * CFW plugin loaders (ARK-4, PRO) pass the PRX path as the start argument.
 * Fall back to the default install path if it is missing or malformed.
 */
static void capture_module_path(SceSize args, const void *argp)
{
    if (argp != NULL && args > 1 && args <= sizeof(g_module_path)) {
        memcpy(g_module_path, argp, args);
        g_module_path[args - 1] = '\0';
        if (strchr(g_module_path, ':') != NULL && strrchr(g_module_path, '/') != NULL)
            return;
    }
    strcpy(g_module_path, PW_DEFAULT_PATH);
}

static int main_thread(SceSize args, void *argp)
{
    (void)args;
    (void)argp;

    pw_log_init(g_module_path);
    pw_log("Pocketwatch %s loaded from %s", PW_VERSION, g_module_path);

    /* Complications (States, ...) are started from here. */

    sceKernelExitDeleteThread(0);
    return 0;
}

int module_start(SceSize args, void *argp)
{
    capture_module_path(args, argp);

    SceUID thid = sceKernelCreateThread("PocketwatchMain", main_thread, PW_THREAD_PRIORITY,
                                        PW_THREAD_STACK, 0, NULL);
    if (thid >= 0)
        sceKernelStartThread(thid, 0, NULL);

    return 0;
}

int module_stop(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    return 0;
}

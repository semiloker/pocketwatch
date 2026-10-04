// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Pocketwatch - Pause. Save. Play.
 * Kernel-mode PRX entry point, loaded by the CFW for retail games (UMD/ISO runlevel).
 */
#include <pspkernel.h>
#include <string.h>

#include "core/log.h"
#include "core/selfcheck.h"

#ifdef PW_DEBUG
#include "debug/probe.h"
#endif

PSP_MODULE_INFO("Pocketwatch", PSP_MODULE_KERNEL, 0, 1);

#ifndef PW_VERSION
#define PW_VERSION "dev"
#endif

#define PW_DEFAULT_PATH    "ms0:/seplugins/pocketwatch/pocketwatch.prx"
#define PW_THREAD_PRIORITY 0x20
#define PW_THREAD_STACK    0x2000 /* stb_sprintf alone needs ~0.6 KB per call */
#define PW_STOP_TIMEOUT_US (500 * 1000)

static char g_module_path[256];
static SceUID g_main_thread = -1;

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
    pw_selfcheck_imports();

#ifdef PW_DEBUG
    pw_probe_boot();
    pw_probe_start();
#endif

    /* Complications (States, ...) are started from here. */

    return 0;
}

int module_start(SceSize args, void *argp)
{
    capture_module_path(args, argp);

    g_main_thread = sceKernelCreateThread("PocketwatchMain", main_thread, PW_THREAD_PRIORITY,
                                          PW_THREAD_STACK, 0, NULL);
    if (g_main_thread >= 0)
        sceKernelStartThread(g_main_thread, 0, NULL);

    return 0;
}

/*
 * Called when the game exits (the kernel unloads every plugin). No thread of ours may outlive
 * this function: a thread left running in unloaded memory hangs the exit on "Please wait...".
 * No file I/O here either - the system is already tearing down.
 */
int module_stop(SceSize args, void *argp)
{
    (void)args;
    (void)argp;

#ifdef PW_DEBUG
    pw_probe_stop();
#endif

    if (g_main_thread >= 0) {
        SceUInt timeout = PW_STOP_TIMEOUT_US;

        if (sceKernelWaitThreadEnd(g_main_thread, &timeout) < 0)
            sceKernelTerminateThread(g_main_thread);
        sceKernelDeleteThread(g_main_thread);
        g_main_thread = -1;
    }
    return 0;
}

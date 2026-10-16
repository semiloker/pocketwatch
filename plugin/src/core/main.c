// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Pocketwatch - Pause. Save. Play.
 * Kernel-mode PRX entry point, loaded by the CFW for retail games (UMD/ISO runlevel).
 */
#include <pspkernel.h>
#include <string.h>

#include "complications/states/engine.h"
#include "core/crc32.h"
#include "core/log.h"
#include "core/mem.h"
#include "core/platform.h"
#include "core/selfcheck.h"
#include "core/sysinfo.h"
#include "core/game.h"
#include "complications/states/store.h"
#include "gfx/assets.h"
#include "shell/shell.h"
#include "ui/lang.h"
#include "ui/settings.h"

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

/*
 * The CRC of our own PRX file: part of every state's environment. A state's kernel image holds the
 * module that made it, so it loads only into exactly that build (the relocated code in RAM differs
 * between boots, the file does not).
 */
static unsigned int module_file_crc(void)
{
    pw_mem_block mem;
    unsigned int crc = 0;
    int fd, n;

    if (pw_mem_alloc(&mem, "PwPrxCrc", 64 * 1024) < 0)
        return 0;
    fd = pw_file_open(g_module_path, 0);
    if (fd >= 0) {
        while ((n = pw_file_get(fd, mem.ptr, mem.size)) > 0)
            crc = pw_crc32(crc, mem.ptr, (unsigned int)n);
        pw_file_close(fd);
    }
    pw_mem_free(&mem);
    return crc;
}

static int main_thread(SceSize args, void *argp)
{
    (void)args;
    (void)argp;

    pw_log_init(g_module_path);
    pw_log("Pocketwatch %s loaded from %s", PW_VERSION, g_module_path);
    pw_selfcheck_imports();

    int ret = pw_engine_init();
    if (ret < 0)
        pw_log("engine: sysevent handler registration failed 0x%08X", ret);
    pw_settings_init(); /* config.txt: fast mode, the status corner */

#ifdef PW_DEBUG
    pw_probe_boot();
    pw_probe_start();
#endif

    /* Complications (States, ...) are started from here. */
    pw_sysinfo_init(); /* date/time format and theme colour from the registry, read only */
    pw_assets_init();  /* the system's font, icons and skies: the menu must not wait for flash0 */
    pw_engine_set_env_extra(0, pw_assets_base()); /* a state must find this block where it was */
    pw_engine_set_env_extra(1, module_file_crc());
    pw_lang_init(); /* after the fonts: it checks their glyphs; before the store (SFO titles) */
    pw_game_init();
    pw_store_init(); /* the slots' hashed PARAM.SFO, while no game thread holds anything */
    ret = pw_shell_start();
    if (ret < 0)
        pw_log("shell: thread start failed 0x%08X", ret);

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

    pw_shell_stop();
#ifdef PW_DEBUG
    pw_probe_stop();
#endif
    pw_engine_shutdown();
    pw_assets_shutdown();
    pw_store_shutdown();

    if (g_main_thread >= 0) {
        SceUInt timeout = PW_STOP_TIMEOUT_US;

        if (sceKernelWaitThreadEnd(g_main_thread, &timeout) < 0)
            sceKernelTerminateThread(g_main_thread);
        sceKernelDeleteThread(g_main_thread);
        g_main_thread = -1;
    }
    return 0;
}

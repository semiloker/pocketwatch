// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspctrl.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <string.h>

#include "core/bootpick.h"
#include "core/config.h"
#include "core/platform.h"
#include "stb_sprintf.h"
#include "xboot.h"
#include "xlog.h"
#include "xsettings.h"

#define XMB_ONCE PWX_DIR "xmb-once"     /* Home's Exit: the XMB this one time */
#define PENDING  PWX_DIR "boot-pending" /* Home removes it once its first screen is up */

/* ARK-4 (src/stubs.S): SystemCtrlForUser, KUBridge. */
void *sctrlArkGetConfig(void *conf);
unsigned int sctrlHENFindFunction(const char *module, const char *library, unsigned int nid);
struct pwx_kcall {
    unsigned int arg[12], ret1, ret2;
};
int kuKernelCall(void *func, struct pwx_kcall *args);

/* sctrlArkSetConfig: a kernel-only export (SystemCtrlPrivate), called the way ARK-4's xmbctrl
   does (extras/modules/xmbctrl/main.c, exec_custom_launcher). */
#define NID_ARK_SET_CONFIG 0x6EAFC03D

static int exists(const char *path)
{
    SceIoStat st;

    return sceIoGetstat(path, &st) >= 0;
}

/* Take a marker file away; 1 if it was there. */
static int take(const char *path)
{
    if (!exists(path))
        return 0;
    sceIoRemove(path);
    return 1;
}

int pwx_boot(void)
{
    static unsigned char cfg[PW_ARK_CONFIG_BUF];
    pw_boot_in in;
    SceCtrlData pad;
    char line[160], path[160];
    const char *why = "";
    int home, ark = 0;

    memset(&in, 0, sizeof(in));
    pw_config_load(PWX_DIR "config.txt");
    in.want_home = pw_config_get()->start == 1;
    in.xmb_once = take(XMB_ONCE); /* always: stale markers must not wait for the next start */
    in.pending = take(PENDING);
    if (in.want_home) {
        memset(&pad, 0, sizeof(pad));
        sceCtrlPeekBufferPositive(&pad, 1);
        in.held = (pad.Buttons & PSP_CTRL_LTRIGGER) != 0;
        memset(cfg, 0, sizeof(cfg));
        sctrlArkGetConfig(cfg);
        ark = pw_boot_ark_check(cfg);
        in.ark_ok = ark == 0;
        if (in.ark_ok) {
            stbsp_snprintf(path, sizeof(path), "%s%s", pw_boot_ark_folder(cfg), PW_BOOT_HOME_FILE);
            in.home_present = exists(path);
        }
    }
    home = pw_boot_pick(&in, &why);
    if (home) {
        void *set =
            (void *)sctrlHENFindFunction("SystemControl", "SystemCtrlPrivate", NID_ARK_SET_CONFIG);
        struct pwx_kcall a;

        memset(&a, 0, sizeof(a));
        a.arg[0] = (unsigned int)cfg;
        if (set && pw_boot_ark_set(cfg, PW_BOOT_HOME_FILE) == 0 &&
            pw_file_write(PENDING, "1", 1, 0) == 1 && kuKernelCall(set, &a) >= 0) {
            why = path;
        } else {
            sceIoRemove(PENDING);
            home = 0;
            why = "ARK-4's config could not be set";
        }
    }
    stbsp_snprintf(line, sizeof(line), "start: %s (%s), ARK-4 config check %d\n",
                   home ? "Pocketwatch Home" : "XMB", why, ark);
    pw_file_write(PWX_DIR "boot.txt", line, (unsigned int)strlen(line), 0);
    pwx_log("%.*s", (int)strlen(line) - 1, line);
    return home;
}

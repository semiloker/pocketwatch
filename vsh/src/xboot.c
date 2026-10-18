// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspctrl.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <psprtc.h>
#include <string.h>

#include "core/bootpick.h"
#include "core/config.h"
#include "core/platform.h"
#include "stb_sprintf.h"
#include "xboot.h"
#include "xlog.h"
#include "xsettings.h"

#define XMB_ONCE PWX_DIR "xmb-once"     /* Home's Exit: the RTC tick it was written at */
#define PENDING  PWX_DIR "boot-pending" /* Home removes it once its first screen is up */
#define BOOT_LOG PWX_DIR "boot.txt"
#define LOG_KEEP 560 /* bytes of earlier decisions kept in boot.txt (about 5 lines) */

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

static unsigned char g_cfg[PW_ARK_CONFIG_BUF];
static int g_home;    /* Home was chosen; the field is still to be written */
static int g_applied; /* 1 written, < 0 refused (the check's result) */
static int g_xmb;     /* the XMB started although Home was chosen */

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

/* The XMB-once marker counts only for the start right after Home wrote it. */
static int take_xmb_once(void)
{
    char text[24];
    int n = pw_file_read(XMB_ONCE, text, sizeof(text) - 1);
    u64 now = 0;

    if (n < 0)
        return 0;
    sceIoRemove(XMB_ONCE);
    text[n > 0 ? n : 0] = 0;
    sceRtcGetCurrentTick(&now);
    return pw_boot_marker_fresh(text, now);
}

/* boot.txt: the last decisions, newest last. */
static void log_decision(const char *line)
{
    static char buf[LOG_KEEP + 200];
    int n = pw_file_read(BOOT_LOG, buf, LOG_KEEP), keep = 0;

    if (n > 0) { /* drop whole lines from the front until it fits */
        keep = n;
        while (keep > LOG_KEEP - 120) {
            char *nl = memchr(buf + (n - keep), '\n', (unsigned int)keep);

            if (!nl)
                break;
            keep = n - (int)(nl + 1 - buf);
        }
        memmove(buf, buf + (n - keep), (unsigned int)keep);
    }
    stbsp_snprintf(buf + keep, (int)sizeof(buf) - keep, "%s\n", line);
    pw_file_write(BOOT_LOG, buf, (unsigned int)strlen(buf), 0);
}

void pwx_boot_decide(void)
{
    pw_boot_in in;
    SceCtrlData pad;
    ScePspDateTime t;
    char line[200], path[160];
    const char *why = "";
    int layout = 0;

    memset(&in, 0, sizeof(in));
    pw_config_load(PWX_DIR "config.txt");
    in.want_home = pw_config_get()->start == 1;
    in.xmb_once = take_xmb_once(); /* always: stale markers must not wait for the next start */
    in.pending = take(PENDING);
    if (in.want_home) {
        memset(&pad, 0, sizeof(pad));
        sceCtrlPeekBufferPositive(&pad, 1);
        in.held = (pad.Buttons & PSP_CTRL_LTRIGGER) != 0;
        sctrlArkGetConfig(g_cfg);
        layout = pw_boot_ark_layout(g_cfg);
        in.ark_ok = layout == 0;
        if (in.ark_ok) {
            stbsp_snprintf(path, sizeof(path), "%s%s", pw_boot_ark_folder(g_cfg),
                           PW_BOOT_HOME_FILE);
            in.home_present = exists(path);
        }
    }
    g_home = pw_boot_pick(&in, &why);
    if (g_home && pw_file_write(PENDING, "1", 1, 0) != 1) {
        g_home = 0;
        why = "boot-pending could not be written";
    }
    memset(&t, 0, sizeof(t));
    sceRtcGetCurrentClockLocalTime(&t);
    stbsp_snprintf(line, sizeof(line), "%04d-%02d-%02d %02d:%02d:%02d start: %s (%s), layout %d",
                   t.year, t.month, t.day, t.hour, t.minute, t.second,
                   g_home ? "Pocketwatch Home" : "XMB", why, layout);
    log_decision(line);
    pwx_log("%s", line);
}

void pwx_boot_module(const char *modname)
{
    struct pwx_kcall a;
    void *set;
    int r;

    if (!g_home || g_applied || strcmp(modname, "sceMediaSync") != 0)
        return;
    /* ARK-4's processSettings has just run (an older handler): read the config as it is now. */
    memset(g_cfg, 0, sizeof(g_cfg));
    sctrlArkGetConfig(g_cfg);
    r = pw_boot_ark_check(g_cfg);
    set = (void *)sctrlHENFindFunction("SystemControl", "SystemCtrlPrivate", NID_ARK_SET_CONFIG);
    if (r == 0 && !set)
        r = -10;
    if (r == 0 && pw_boot_ark_set(g_cfg, PW_BOOT_HOME_FILE) < 0)
        r = -11;
    if (r == 0) {
        memset(&a, 0, sizeof(a));
        a.arg[0] = (unsigned int)g_cfg;
        if (kuKernelCall(set, &a) < 0)
            r = -12;
    }
    g_applied = r == 0 ? 1 : r;
}

void pwx_boot_xmb_starts(void)
{
    if (!g_home)
        return;
    g_xmb = 1;
    memset(g_cfg, 0, sizeof(g_cfg));
    sctrlArkGetConfig(g_cfg);
    pwx_log("start: Home was chosen but the XMB starts (written %d, launcher \"%.19s\")", g_applied,
            (const char *)g_cfg + PW_ARK_LAUNCH_OFF);
}

void pwx_boot_tidy(void)
{
    if (g_xmb && g_xmb++ == 1) {
        sceIoRemove(PENDING);
        log_decision("  ... but the XMB started (see xmb.log)");
    }
}

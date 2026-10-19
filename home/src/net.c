// SPDX-License-Identifier: GPL-3.0-or-later
/* The order of the steps follows PSPSDK's htmlviewer sample (samples/utility/htmlviewer); the
   browser's memory comes from Home's heap (Home leaves only 4 MB outside it, ADR-025). */
#include <malloc.h>
#include <pspkernel.h>
#include <pspnet.h>
#include <pspnet_apctl.h>
#include <pspnet_inet.h>
#include <pspnet_resolver.h>
#include <psphttp.h>
#include <pspssl.h>
#include <psputility.h>
#include <psputility_htmlviewer.h>
#include <psputility_modules.h>
#include <pspwlan.h>
#include <string.h>

#include "net.h"
#include "stb_sprintf.h"

static const int k_modules[] = {PSP_MODULE_NET_COMMON,    PSP_MODULE_NET_INET,
                                PSP_MODULE_NET_PARSEURI,  PSP_MODULE_NET_PARSEHTTP,
                                PSP_MODULE_NET_HTTP,      PSP_MODULE_NET_SSL};
#define MODULES ((int)(sizeof(k_modules) / sizeof(k_modules[0])))

/* The memory the browser is given, largest first: the sample's 10 MB, then less. */
static const unsigned int k_pool_mb[] = {10, 8, 6};

enum { UP_NONE, UP_NET, UP_INET, UP_RESOLVER, UP_APCTL, UP_SSL, UP_HTTP, UP_HTTPS };

static char *g_log;
static int g_cap, g_len;

static void note(const char *what, int r)
{
    if (g_log && g_len < g_cap - 1)
        g_len += stbsp_snprintf(g_log + g_len, g_cap - g_len, r < 0 ? " %s=0x%08X" : " %s=%d",
                                what, r);
}

/* Everything up to step `up` let go again, in the reverse order. */
static void down(int up, int modules)
{
    if (up >= UP_HTTPS) {
        sceHttpSaveSystemCookie();
        sceHttpsEnd();
    }
    if (up >= UP_HTTP)
        sceHttpEnd();
    if (up >= UP_SSL)
        sceSslEnd();
    if (up >= UP_APCTL)
        sceNetApctlTerm();
    if (up >= UP_RESOLVER)
        sceNetResolverTerm();
    if (up >= UP_INET)
        sceNetInetTerm();
    if (up >= UP_NET)
        sceNetTerm();
    while (modules > 0)
        sceUtilityUnloadModule(k_modules[--modules]);
}

static void html_update(void)
{
    sceUtilityHtmlViewerUpdate(1);
}
static void html_shutdown(void)
{
    sceUtilityHtmlViewerShutdownStart();
}

int hm_net_browser(const char *url, int cross_enter, int language, hm_frame_fn frame, void *ctx,
                   char *log, int cap)
{
    static pspUtilityHtmlViewerParam p;
    static char start[256];
    void *pool = 0;
    unsigned int mb = 0;
    int modules = 0, up = UP_NONE, r = 0;

    g_log = log;
    g_cap = cap;
    g_len = 0;
    if (log && cap > 0)
        log[0] = 0;
    note("switch", sceWlanGetSwitchState());
    note("free_kb", (int)(sceKernelMaxFreeMemSize() >> 10)); /* outside Home's heap */
    for (; modules < MODULES; modules++) {
        r = sceUtilityLoadModule(k_modules[modules]);
        if (r < 0 && r != (int)0x80111102) { /* (already loaded: fine) */
            note("load", r);
            goto fail;
        }
    }
    note("modules_free_kb", (int)(sceKernelMaxFreeMemSize() >> 10));
    if ((r = sceNetInit(0x20000, 0x2A, 0, 0x2A, 0)) < 0)
        goto step;
    up = UP_NET;
    if ((r = sceNetInetInit()) < 0)
        goto step;
    up = UP_INET;
    if ((r = sceNetResolverInit()) < 0)
        goto step;
    up = UP_RESOLVER;
    if ((r = sceNetApctlInit(0x1800, 0x30)) < 0)
        goto step;
    up = UP_APCTL;
    if ((r = sceSslInit(0x28000)) < 0)
        goto step;
    up = UP_SSL;
    if ((r = sceHttpInit(0x25800)) < 0)
        goto step;
    up = UP_HTTP;
    if ((r = sceHttpsInit(0, 0, 0, 0)) < 0)
        goto step;
    up = UP_HTTPS;
    sceHttpsLoadDefaultCert(0, 0);
    sceHttpLoadSystemCookie();

    for (unsigned int i = 0; i < sizeof(k_pool_mb) / sizeof(k_pool_mb[0]) && !pool; i++)
        if ((pool = memalign(64, k_pool_mb[i] << 20)) != 0)
            mb = k_pool_mb[i];
    note("pool_mb", (int)mb);
    if (!pool) {
        r = -1;
        goto fail;
    }
    stbsp_snprintf(start, sizeof(start), "%s", url);
    memset(&p, 0, sizeof(p));
    hm_dialog_base(&p.base, sizeof(p), cross_enter, language);
    p.memaddr = pool;
    p.memsize = mb << 20;
    p.initialurl = start;
    p.homeurl = start;
    p.numtabs = 1;
    p.cookiemode = PSP_UTILITY_HTMLVIEWER_COOKIEMODE_DEFAULT;
    p.textsize = PSP_UTILITY_HTMLVIEWER_TEXTSIZE_NORMAL;
    p.displaymode = PSP_UTILITY_HTMLVIEWER_DISPLAYMODE_SMART_FIT;
    p.options = PSP_UTILITY_HTMLVIEWER_DISABLE_STARTUP_LIMITS;
    p.interfacemode = PSP_UTILITY_HTMLVIEWER_INTERFACEMODE_FULL;
    p.connectmode = PSP_UTILITY_HTMLVIEWER_CONNECTMODE_MANUAL_ONCE; /* the XMB's list, once */
    p.dldirname = "/PSP/COMMON"; /* (no "ms0:": the firmware adds it) */
    r = sceUtilityHtmlViewerInitStart(&p);
    note("start", r);
    if (r < 0)
        goto fail;
    hm_dialog_run(sceUtilityHtmlViewerGetStatus, html_update, html_shutdown, frame, ctx);
    note("used_kb", (int)(p.memused >> 10));
    free(pool);
    down(up, modules);
    return 0;

step:
    note("init", r);
    note("step", up);
fail:
    free(pool);
    down(up, modules);
    return r < 0 ? r : -1;
}

int hm_net_switch(void)
{
    return sceWlanGetSwitchState();
}

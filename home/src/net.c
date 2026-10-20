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
#include <psputility_netmodules.h>
#include <psputility_netparam.h>
#include <pspwlan.h>
#include <netinet/in.h>
#include <string.h>

#include "clockfix.h"
#include "net.h"
#include "stb_sprintf.h"

static const int k_modules[] = {PSP_MODULE_NET_COMMON,   PSP_MODULE_NET_INET,
                                PSP_MODULE_NET_PARSEURI, PSP_MODULE_NET_PARSEHTTP,
                                PSP_MODULE_NET_HTTP,     PSP_MODULE_NET_SSL};
#define MODULES ((int)(sizeof(k_modules) / sizeof(k_modules[0])))

/* The memory the browser is given, largest first: the sample's 10 MB, then less. */
static const unsigned int k_pool_mb[] = {10, 8, 6};

enum { UP_NONE, UP_NET, UP_INET, UP_RESOLVER, UP_APCTL, UP_SSL, UP_HTTP, UP_HTTPS };

static char *g_log;
static int g_cap, g_len;

static void note(const char *what, int r)
{
    if (g_log && g_len < g_cap - 1)
        g_len +=
            stbsp_snprintf(g_log + g_len, g_cap - g_len, r < 0 ? " %s=0x%08X" : " %s=%d", what, r);
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

/* ---- the internet's time (HOM-REQ-032, ADR-041): SNTP over the first saved connection ---- */

#define CONNECT_MS 20000
#define ANSWER_MS  3000

static const char *const k_ntp_servers[] = {"pool.ntp.org", "time.google.com"};

static void wait_ms(unsigned int ms)
{
    sceKernelDelayThread(ms * 1000);
}

/* One question to one server: 0 and the time it answered (Unix microseconds, half the round trip
   added), or < 0. */
static int ask(const char *server, unsigned long long *unix_us)
{
    struct sockaddr_in to;
    struct in_addr addr;
    unsigned char pkt[HM_NTP_BYTES];
    char buf[1024];
    int rid = -1, s, r, one = 1;
    unsigned long long sent, back = 0;

    if ((r = sceNetResolverCreate(&rid, buf, sizeof(buf))) < 0) {
        note("resolver", r);
        return -1;
    }
    r = sceNetResolverStartNtoA(rid, server, &addr, 2, 3);
    sceNetResolverDelete(rid);
    if (r < 0) {
        note("dns", r);
        return -1;
    }
    if ((s = sceNetInetSocket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        note("socket", s);
        return -1;
    }
    sceNetInetSetsockopt(s, SOL_SOCKET, SO_NONBLOCK, &one, sizeof(one)); /* (polled below) */
    memset(&to, 0, sizeof(to));
    to.sin_len = sizeof(to);
    to.sin_family = AF_INET;
    to.sin_port = htons(123);
    to.sin_addr = addr;
    hm_ntp_request(pkt);
    sent = sceKernelGetSystemTimeWide();
    r = (int)sceNetInetSendto(s, pkt, sizeof(pkt), 0, (struct sockaddr *)&to, sizeof(to));
    if (r < 0) {
        note("send", r);
        sceNetInetClose(s);
        return -1;
    }
    r = -1;
    for (unsigned int t = 0; t < ANSWER_MS; t += 50) {
        int n = (int)sceNetInetRecvfrom(s, pkt, sizeof(pkt), 0, 0, 0);

        if (n >= HM_NTP_BYTES) {
            back = sceKernelGetSystemTimeWide();
            if ((r = hm_ntp_parse(pkt, n, unix_us)) == 0)
                *unix_us += (back - sent) / 2;
            else
                note("answer", r);
            break;
        }
        wait_ms(50);
    }
    if (!back)
        note("timeout", ANSWER_MS);
    sceNetInetClose(s);
    return r < 0 ? -1 : 0;
}

int hm_net_time(unsigned long long *unix_us, char *log, int cap)
{
    int r, profile = 0, up = UP_NONE, common = 0, inet = 0, state = 0;

    g_log = log;
    g_cap = cap;
    g_len = 0;
    if (log && cap > 0)
        log[0] = 0;
    r = sceWlanGetSwitchState();
    note("switch", r);
    if (r == 0)
        return HM_NET_SWITCH_OFF;
    for (int i = 1; i <= 10 && !profile; i++) /* the XMB's Network Settings: the first one */
        if (sceUtilityCheckNetParam(i) == 0)
            profile = i;
    note("profile", profile);
    if (!profile)
        return HM_NET_NO_PROFILE;
    /* NTPSP's order (joel16/NTPSP, app/source/net.c): the older call that loads the network's
       modules, and the stack sizes it gives sceNetInit - the browser's way failed in sceNetInit
       with 0x8002013A on the owner's PSP (home.log 2026-10-20) */
    if ((r = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON)) >= 0)
        common = 1;
    note("common", r);
    if ((r = sceUtilityLoadNetModule(PSP_NET_MODULE_INET)) >= 0)
        inet = 1;
    note("inet_mod", r);
    if ((r = sceNetInit(128 * 1024, 42, 4 * 1024, 42, 4 * 1024)) < 0)
        goto step;
    up = UP_NET;
    if ((r = sceNetInetInit()) < 0)
        goto step;
    up = UP_INET;
    if ((r = sceNetResolverInit()) < 0)
        goto step;
    up = UP_RESOLVER;
    if ((r = sceNetApctlInit(0x8000, 48)) < 0)
        goto step;
    up = UP_APCTL;
    if ((r = sceNetApctlConnect(profile)) < 0)
        goto step;
    for (unsigned int t = 0; t < CONNECT_MS; t += 100) {
        int was = state;

        if (sceNetApctlGetState(&state) < 0 || state == PSP_NET_APCTL_STATE_GOT_IP)
            break;
        if (was > PSP_NET_APCTL_STATE_DISCONNECTED && state == PSP_NET_APCTL_STATE_DISCONNECTED)
            break; /* it gave up */
        wait_ms(100);
    }
    note("apctl", state);
    if (state != PSP_NET_APCTL_STATE_GOT_IP) {
        r = HM_NET_NO_CONNECTION;
        goto fail;
    }
    r = -1;
    for (unsigned int i = 0; i < sizeof(k_ntp_servers) / sizeof(k_ntp_servers[0]) && r < 0; i++)
        r = ask(k_ntp_servers[i], unix_us);
    sceNetApctlDisconnect();
    goto fail; /* (the same way down, with r) */

step:
    note("init", r);
    note("step", up);
fail:
    down(up, 0);
    if (inet)
        sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
    if (common)
        sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
    return r == 0 || r == HM_NET_NO_CONNECTION ? r : HM_NET_FAILED;
}

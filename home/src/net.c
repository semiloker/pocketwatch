// SPDX-License-Identifier: GPL-3.0-or-later
/* The order of the steps follows PSPSDK's htmlviewer sample (samples/utility/htmlviewer) for the
   browser, NTPSP's (joel16/NTPSP, app/source/net.c) for the rest; the browser's memory comes
   from Home's heap (Home leaves only 4 MB outside it, ADR-025). */
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
#include "hw.h"
#include "net.h"
#include "stb_sprintf.h"

/* ARK-4 (src/stubs.S): a function by its library and NID; a kernel function run for us. */
unsigned int sctrlHENFindFunction(const char *module, const char *library, unsigned int nid);
struct hm_kcall {
    unsigned int arg[12], ret1, ret2;
};
int kuKernelCall(void *func, struct hm_kcall *args);

/* The search: sceNetApctl's, past PSPSDK's library (src/apctl_scan.S). */
int sceNetApctlScanUser(void);
int sceNetApctlGetBSSDescIDListUser(unsigned int *size, void *buf);
int sceNetApctlGetBSSDescEntryUser(int entry, int info, void *out);
#define DESC_SSID     1 /* 32 bytes */
#define DESC_SSID_LEN 2 /* a word */
#define DESC_CHANNEL  3 /* a byte */
#define DESC_STRENGTH 4 /* a byte, 0-100 */
#define DESC_SECURITY 5 /* a word: HM_WIFI_SEC_* */

/* sceUtilitySetNetParam's parameters past PSPSDK's list (PPSSPP Core/HLE/sceUtility.h) */
#define NETPARAM_WPA_TYPE 21 /* 0 the key in hex, 1 a passphrase */
#define NETPARAM_WPA_KEY  22 /* 64 bytes */

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

/* ---- the network up and down again (NTPSP's way: the older call that loads the modules, and
   the stack sizes it gives sceNetInit) ---- */

typedef struct {
    int up, common, inet;
} net_st;

static int net_up(net_st *s)
{
    int r;

    memset(s, 0, sizeof(*s));
    if ((r = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON)) >= 0)
        s->common = 1;
    note("common", r);
    if ((r = sceUtilityLoadNetModule(PSP_NET_MODULE_INET)) >= 0)
        s->inet = 1;
    note("inet_mod", r);
    if ((r = sceNetInit(128 * 1024, 42, 4 * 1024, 42, 4 * 1024)) < 0)
        goto step;
    s->up = UP_NET;
    if ((r = sceNetInetInit()) < 0)
        goto step;
    s->up = UP_INET;
    if ((r = sceNetResolverInit()) < 0)
        goto step;
    s->up = UP_RESOLVER;
    if ((r = sceNetApctlInit(0x8000, 48)) < 0)
        goto step;
    s->up = UP_APCTL;
    return 0;
step:
    note("init", r);
    note("step", s->up);
    return r;
}

static void net_down(net_st *s)
{
    down(s->up, 0);
    if (s->inet)
        sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
    if (s->common)
        sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
}

#define CONNECT_MS 20000
#define ANSWER_MS  3000
#define SCAN_MS    15000

static void wait_ms(unsigned int ms)
{
    sceKernelDelayThread(ms * 1000);
}

/* Through connection slot until it has an address: 0, or HM_NET_*. */
static int net_connect(int slot)
{
    int r, state = 0, reached = 0;

    if ((r = sceNetApctlConnect(slot)) < 0) {
        note("connect", r);
        return HM_NET_FAILED;
    }
    for (unsigned int t = 0; t < CONNECT_MS; t += 100) {
        int was = state;

        if (sceNetApctlGetState(&state) < 0 || state == PSP_NET_APCTL_STATE_GOT_IP)
            break;
        if (state > reached)
            reached = state;
        if (was > PSP_NET_APCTL_STATE_DISCONNECTED && state == PSP_NET_APCTL_STATE_DISCONNECTED)
            break; /* it gave up */
        wait_ms(100);
    }
    note("apctl", state);
    note("reached", reached); /* 2 joining, 3 an address, 6 the key: how far a refusal got */
    return state == PSP_NET_APCTL_STATE_GOT_IP ? 0 : HM_NET_NO_CONNECTION;
}

/* ---- the internet's time (HOM-REQ-032, ADR-041): SNTP ---- */

static const char *const k_ntp_servers[] = {"pool.ntp.org", "time.google.com"};

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

/* Connected: the internet's time as the PSP's clock. 1 when it was set. */
static int set_time(void)
{
    unsigned long long us = 0;
    int r = -1;

    for (unsigned int i = 0; i < sizeof(k_ntp_servers) / sizeof(k_ntp_servers[0]) && r < 0; i++)
        r = ask(k_ntp_servers[i], &us);
    return r == 0 && hm_hw_set_utc(us) == 0;
}

/* ---- the saved connections (the registry, through the firmware's netparam calls) ---- */

int hm_net_connections(hm_wifi_conn *out, int max)
{
    int n = 0;

    for (int i = 1; i <= HM_WIFI_CONNS && n < max; i++) {
        netData d;

        if (sceUtilityCheckNetParam(i) != 0)
            continue;
        memset(&out[n], 0, sizeof(out[n]));
        out[n].slot = i;
        out[n].secure = -1;
        memset(&d, 0, sizeof(d));
        if (sceUtilityGetNetParam(i, PSP_NETPARAM_NAME, &d) == 0)
            stbsp_snprintf(out[n].name, sizeof(out[n].name), "%.63s", d.asString);
        memset(&d, 0, sizeof(d));
        if (sceUtilityGetNetParam(i, PSP_NETPARAM_SSID, &d) == 0)
            stbsp_snprintf(out[n].ssid, sizeof(out[n].ssid), "%.32s", d.asString);
        if (sceUtilityGetNetParam(i, PSP_NETPARAM_SECURE, &d) == 0)
            out[n].secure = (int)d.asUint;
        n++;
    }
    return n;
}

/* sceUtility_netparam_internal (PSPSDK src/utility; the firmware's pspnet_ap_dialog_dummy makes
   its connection with them: yne/js-psp reverse/pspnet_ap_dialog_dummy.c). Not linked for a user
   program: the same call through KUBridge, in kernel mode. */
enum { NP_CREATE, NP_DELETE, NP_COPY, NP_SET };
static const unsigned int k_np_nid[4] = {0x072DEBF2, 0x9CE50172, 0xFB0C4840, 0xFC4516F3};
static int g_np_kernel;

static int np_call(int fn, unsigned int a0, unsigned int a1)
{
    int r = fn == NP_CREATE   ? sceUtilityCreateNetParam((int)a0)
            : fn == NP_DELETE ? sceUtilityDeleteNetParam((int)a0)
            : fn == NP_COPY   ? sceUtilityCopyNetParam((int)a0, (int)a1)
                              : sceUtilitySetNetParam((int)a0, (const void *)a1);

    if (r == (int)0x8002013A) {
        void *f = (void *)sctrlHENFindFunction("sceUtility_Driver", "sceUtility_netparam_internal",
                                               k_np_nid[fn]);
        struct hm_kcall k;

        memset(&k, 0, sizeof(k));
        k.arg[0] = a0;
        k.arg[1] = a1;
        if (((unsigned int)f >> 28) == 8 && kuKernelCall(f, &k) >= 0) /* (kernel memory only) */
            r = (int)k.ret1;
        g_np_kernel = 1;
    }
    return r;
}

int hm_net_delete(int slot)
{
    return slot >= 1 && slot <= HM_WIFI_CONNS ? np_call(NP_DELETE, (unsigned int)slot, 0) : -1;
}

static int set_text(int param, const char *s, unsigned int max)
{
    static char buf[128];

    memset(buf, 0, sizeof(buf));
    memcpy(buf, s, strlen(s) < max ? strlen(s) : max);
    return np_call(NP_SET, (unsigned int)param, (unsigned int)buf);
}

static int set_int(int param, int v)
{
    static int x;

    x = v;
    return np_call(NP_SET, (unsigned int)param, (unsigned int)&x);
}

/* ap as a connection (DHCP, no proxy), its name its SSID; that network's earlier one replaced.
   0 and *slot, or HM_NET_*. */
static int save(const hm_wifi_ap *ap, const char *key, int *slot)
{
    hm_wifi_conn c[HM_WIFI_CONNS];
    int n = hm_net_connections(c, HM_WIFI_CONNS), s = 0, secure = hm_wifi_secure(ap->security),
        hex = 0, r;
    static unsigned char wpa[64];

    if (secure < 0 || (secure >= 3 && (!key || !hm_wifi_key_ok(key, &hex))))
        return HM_NET_NOT_SAVED;
    for (int i = 0; i < n && !s; i++)
        if (strcmp(c[i].ssid, ap->ssid) == 0)
            s = c[i].slot;
    if (s)
        np_call(NP_DELETE, (unsigned int)s, 0);
    for (int i = 1; i <= HM_WIFI_CONNS && !s; i++)
        if (sceUtilityCheckNetParam(i) != 0)
            s = i;
    if (!s)
        return HM_NET_FULL;
    note("slot", s);
    if ((r = np_call(NP_CREATE, (unsigned int)s, 0)) < 0 ||
        (r = set_text(PSP_NETPARAM_NAME, ap->ssid, 63)) < 0 ||
        (r = set_text(PSP_NETPARAM_SSID, ap->ssid, 32)) < 0 ||
        (r = set_int(PSP_NETPARAM_SECURE, secure)) < 0)
        goto fail;
    if (secure >= 3) {
        memset(wpa, 0, sizeof(wpa));
        memcpy(wpa, key, strlen(key));
        r = set_int(NETPARAM_WPA_TYPE, hex ? 0 : 1);
        if (r >= 0)
            r = np_call(NP_SET, NETPARAM_WPA_KEY, (unsigned int)wpa);
        memset(wpa, 0, sizeof(wpa));
        if (r < 0)
            goto fail;
    }
    if ((r = set_int(PSP_NETPARAM_IS_STATIC_IP, 0)) < 0 ||
        (r = set_int(PSP_NETPARAM_MANUAL_DNS, 0)) < 0 ||
        (r = set_int(PSP_NETPARAM_USE_PROXY, 0)) < 0 ||
        (r = np_call(NP_COPY, 0, (unsigned int)s)) < 0)
        goto fail;
    note("secure", secure);
    note("kernel_calls", g_np_kernel);
    *slot = s;
    return 0;
fail:
    note("netparam", r);
    note("kernel_calls", g_np_kernel);
    np_call(NP_DELETE, (unsigned int)s, 0);
    return HM_NET_NOT_SAVED;
}

/* ---- the jobs ---- */

static hm_wifi_ap g_aps[2][HM_WIFI_APS]; /* the one the screen reads, and the next */
static volatile int g_aps_cur, g_naps[2];

static int first_connection(void)
{
    for (int i = 1; i <= HM_WIFI_CONNS; i++) /* the XMB's Network Settings: the first one */
        if (sceUtilityCheckNetParam(i) == 0)
            return i;
    return 0;
}

static int job_scan(hm_net_result *res)
{
    struct {
        unsigned int next, id;
    } list[HM_WIFI_APS + 8];
    unsigned int size = sizeof(list);
    int r, state = 0, seen = 0, n, m, k = g_aps_cur ^ 1;
    hm_wifi_ap *a = g_aps[k];
    net_st st;

    if ((r = net_up(&st)) < 0)
        goto out;
    if ((r = sceNetApctlScanUser()) < 0) {
        note("scan", r);
        goto out;
    }
    for (unsigned int t = 0; t < SCAN_MS; t += 100) { /* scanning, then disconnected again */
        if (sceNetApctlGetState(&state) < 0)
            break;
        if (state == PSP_NET_APCTL_STATE_SCANNING)
            seen = 1;
        else if (seen || t >= 2000)
            break;
        wait_ms(100);
    }
    note("scan_state", state);
    memset(list, 0, sizeof(list));
    if ((r = sceNetApctlGetBSSDescIDListUser(&size, list)) < 0) {
        note("list", r);
        goto out;
    }
    n = (int)(size / sizeof(list[0]));
    note("seen", n);
    n = n > HM_WIFI_APS ? HM_WIFI_APS : n;
    m = 0;
    for (int i = 0; i < n; i++) {
        unsigned char ssid[36], b[4] = {0, 0, 0, 0}, ch[4] = {0, 0, 0, 0};
        unsigned int len = 0, sec = 0;
        int id = (int)list[i].id;

        memset(ssid, 0, sizeof(ssid));
        if (sceNetApctlGetBSSDescEntryUser(id, DESC_SSID, ssid) < 0)
            continue;
        sceNetApctlGetBSSDescEntryUser(id, DESC_SSID_LEN, &len);
        sceNetApctlGetBSSDescEntryUser(id, DESC_STRENGTH, b);
        sceNetApctlGetBSSDescEntryUser(id, DESC_SECURITY, &sec);
        sceNetApctlGetBSSDescEntryUser(id, DESC_CHANNEL, ch);
        if (len == 0 || len > 32)
            len = (unsigned int)strnlen((const char *)ssid, 32);
        memset(&a[m], 0, sizeof(a[m]));
        memcpy(a[m].ssid, ssid, len);
        a[m].strength = b[0] > 100 ? 100 : b[0];
        a[m].security = (int)sec;
        a[m].channel = ch[0];
        m++;
    }
    g_naps[k] = hm_wifi_tidy(a, m);
    g_aps_cur = k;
    res->found = g_naps[k];
    r = 0;
out:
    net_down(&st);
    return r < 0 ? HM_NET_FAILED : 0;
}

/* Connect through res->slot; the time from the internet while there. */
static int job_test(hm_net_result *res)
{
    union SceNetApctlInfo info; /* (the tag: older PSPSDKs have no typedef) */
    netData d;
    net_st st;
    int r;

    if (sceUtilityGetNetParam(res->slot, PSP_NETPARAM_SECURE, &d) == 0) /* (what the XMB keeps) */
        note("secure", (int)d.asUint);
    if (sceUtilityGetNetParam(res->slot, NETPARAM_WPA_TYPE, &d) == 0)
        note("wpa_type", (int)d.asUint);
    if ((r = net_up(&st)) < 0) {
        net_down(&st);
        return HM_NET_FAILED;
    }
    r = net_connect(res->slot);
    if (r == 0) {
        memset(&info, 0, sizeof(info));
        if (sceNetApctlGetInfo(PSP_NET_APCTL_INFO_IP, &info) == 0)
            stbsp_snprintf(res->ip, sizeof(res->ip), "%.15s", info.ip);
        res->time_set = set_time();
        note("time", res->time_set);
        sceNetApctlDisconnect();
    }
    net_down(&st);
    return r;
}

static int job_time(hm_net_result *res)
{
    int r;

    if ((res->slot = first_connection()) == 0)
        return HM_NET_NO_PROFILE;
    if ((r = job_test(res)) < 0)
        return r;
    return res->time_set ? 0 : HM_NET_FAILED;
}

static volatile int g_job_state; /* 0 none, 1 running, 2 finished (not yet said) */
static hm_net_result g_res;
static hm_wifi_ap g_job_ap;
static char g_job_key[HM_WIFI_KEY];
static char g_job_log[400];

static int job_thread(SceSize args, void *argp)
{
    static const char *const names[] = {"-", "time", "search", "join", "test"};
    char line[480];
    int r, sw = sceWlanGetSwitchState();

    (void)args;
    (void)argp;
    g_log = g_job_log;
    g_cap = sizeof(g_job_log);
    g_len = 0;
    g_job_log[0] = 0;
    note("switch", sw);
    if (sw == 0)
        r = HM_NET_SWITCH_OFF;
    else if (g_res.kind == HM_JOB_TIME)
        r = job_time(&g_res);
    else if (g_res.kind == HM_JOB_SCAN)
        r = job_scan(&g_res);
    else if (g_res.kind == HM_JOB_JOIN) /* saved, then tried */
        r = (r = save(&g_job_ap, g_job_key[0] ? g_job_key : 0, &g_res.slot)) == 0 ? job_test(&g_res)
                                                                                  : r;
    else
        r = job_test(&g_res);
    memset(g_job_key, 0, sizeof(g_job_key)); /* (the password is never logged nor kept) */
    g_res.ret = r;
    stbsp_snprintf(line, sizeof(line), "home net: %s %d:%s\n", names[g_res.kind], r, g_job_log);
    hm_hw_log(line);
    g_job_state = 2;
    return sceKernelExitDeleteThread(0);
}

int hm_net_job(int kind, int slot, const hm_wifi_ap *ap, const char *key)
{
    SceUID th;

    if (g_job_state == 1 || kind <= HM_JOB_NONE || kind > HM_JOB_TEST)
        return -1;
    memset(&g_res, 0, sizeof(g_res));
    g_res.kind = kind;
    g_res.slot = slot;
    if (ap)
        g_job_ap = *ap;
    stbsp_snprintf(g_job_key, sizeof(g_job_key), "%s", key ? key : "");
    g_job_state = 1;
    th = sceKernelCreateThread("PocketwatchHomeNet", job_thread, 0x30, 0x8000, 0, 0);
    if (th < 0 || sceKernelStartThread(th, 0, 0) < 0) {
        g_res.ret = HM_NET_FAILED;
        g_job_state = 2;
    }
    return 0;
}

int hm_net_job_poll(hm_net_result *out)
{
    int s = g_job_state;

    *out = g_res;
    if (s == 2)
        g_job_state = 0;
    return s;
}

int hm_net_aps(hm_wifi_ap *out, int max)
{
    int k = g_aps_cur, n = g_naps[k] < max ? g_naps[k] : max;

    memcpy(out, g_aps[k], (size_t)n * sizeof(*out));
    return n;
}

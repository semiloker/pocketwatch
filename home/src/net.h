// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_NET_H
#define HM_NET_H

#include "dialogs.h"
#include "wifi_list.h"

/*
 * The network in Home (HOM-REQ-017, HOM-REQ-032, HOM-REQ-033): the system's Internet Browser
 * (sceUtilityHtmlViewer, the firmware's NetFront), the internet's time, and Wi-Fi - the PSP's
 * saved connections (the XMB's Network Settings: the same list), a search for networks, a new
 * connection from one of them, a test of one. The network's libraries are loaded for each of
 * these and let go after it, so Home keeps nothing of them while browsing games.
 *
 * sceNetInit answered 0x8002013A (not linked) until 2026-10-20: Home's sceNetInet import table
 * was built over sceNet's and sceWlanDrv's stubs (stubs out of order at the link: Makefile), so
 * loading the inet library unlinked them (ADR-044; tools/check_prx.py fails on it now).
 */

/* Run the browser on url until the user leaves it; frame draws behind it. Writes what happened
   (each step's result, the memory it got) into log (cap bytes). Returns 0 when it ran, < 0 when
   it could not start (log says why). */
int hm_net_browser(const char *url, int cross_enter, int language, hm_frame_fn frame, void *ctx,
                   char *log, int cap);

/* The WLAN switch: 1 on, 0 off, < 0 not known. */
int hm_net_switch(void);

enum {
    HM_NET_SWITCH_OFF = -2,
    HM_NET_NO_PROFILE = -3,
    HM_NET_NO_CONNECTION = -4, /* the access point refused or gave no address (the password?) */
    HM_NET_FAILED = -5,
    HM_NET_NOT_SAVED = -6, /* the connection could not be saved */
    HM_NET_FULL = -7,      /* ten connections saved already */
};

/* The saved connections (sceUtilityCheckNetParam / GetNetParam 1-10): read at once (the
   registry; no network). */
#define HM_WIFI_CONNS 10
typedef struct {
    int slot; /* 1-10 */
    char name[64], ssid[HM_WIFI_SSID];
    int secure; /* PSP_NETPARAM_SECURE */
} hm_wifi_conn;
int hm_net_connections(hm_wifi_conn *out, int max);
int hm_net_delete(int slot); /* 0, or < 0 */

/* The network's work: one job at a time, on a thread of its own (each blocks for seconds; the
   screen goes on). TIME: the internet's time through the first connection, set as the PSP's
   clock. SCAN: the networks around (hm_net_aps). JOIN: ap saved as a new connection (key its
   WPA key, or NULL), then tested. TEST: connect through slot, get an address, set the clock from
   the internet while there. Each writes a "home net:" line into home.log. Returns 0, or -1 while
   a job runs. */
enum { HM_JOB_NONE, HM_JOB_TIME, HM_JOB_SCAN, HM_JOB_JOIN, HM_JOB_TEST };
int hm_net_job(int kind, int slot, const hm_wifi_ap *ap, const char *key);

typedef struct {
    int kind, ret; /* HM_JOB_*; 0 or HM_NET_* */
    int slot;      /* JOIN / TEST: the connection */
    char ip[16];   /* the address it got */
    int time_set;  /* the clock was set from the internet */
    int found;     /* SCAN: networks */
} hm_net_result;

/* 0 nothing, 1 a job runs (out->kind its kind), 2 one finished: out filled - said once. */
int hm_net_job_poll(hm_net_result *out);
/* The last search's networks (tidied: hm_wifi_tidy): how many. */
int hm_net_aps(hm_wifi_ap *out, int max);

#endif

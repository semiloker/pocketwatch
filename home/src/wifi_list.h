// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HM_WIFI_LIST_H
#define HM_WIFI_LIST_H

/*
 * Wi-Fi in Home (HOM-REQ-033, ADR-044): the networks a search found and what a new connection
 * needs from them. Portable (tests/test_wifi.c); net.c talks to the firmware.
 */

#define HM_WIFI_SSID 33 /* 32 bytes and the end */
#define HM_WIFI_APS  24
#define HM_WIFI_KEY  65 /* WPA: 8-63 characters, or 64 hex digits */

/* A network's security as sceNetApctl reports it (PSP_NET_APCTL_INFO_SECURITY_TYPE_*: PPSSPP
   Core/HLE/sceNetApctl.h). */
enum {
    HM_WIFI_SEC_NONE,
    HM_WIFI_SEC_WEP,
    HM_WIFI_SEC_WPA_TKIP,
    HM_WIFI_SEC_UNSUPPORTED, /* (WPA-Enterprise and the like) */
    HM_WIFI_SEC_WPA_AES,     /* WPA2-PSK too */
};

typedef struct {
    char ssid[HM_WIFI_SSID];
    int strength; /* 0-100 */
    int security; /* HM_WIFI_SEC_* */
    int channel;
} hm_wifi_ap;

/* The list as shown: hidden networks (no name) out, each name once (its strongest), strongest
   first. Returns how many are left. */
int hm_wifi_tidy(hm_wifi_ap *aps, int n);

/* The connection's security (sceUtilitySetNetParam PSP_NETPARAM_SECURE: 0 none, 1-2 WEP, 3
   WPA-PSK TKIP, 4 WPA-PSK AES) for a network's, or -1 when Home does not make one (WEP: the XMB's
   Network Settings; unsupported). */
int hm_wifi_secure(int security);

/* 1 when key is a WPA key the PSP takes: 8-63 printable ASCII characters (*hex 0), or 64 hex
   digits (*hex 1). */
int hm_wifi_key_ok(const char *key, int *hex);

#endif

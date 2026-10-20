// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "wifi_list.h"

int hm_wifi_tidy(hm_wifi_ap *aps, int n)
{
    int k = 0;

    for (int i = 0; i < n; i++) {
        int j;

        if (!aps[i].ssid[0])
            continue;
        for (j = 0; j < k && strcmp(aps[j].ssid, aps[i].ssid) != 0; j++)
            ;
        if (j < k) { /* the same name again (another access point of it): the stronger stays */
            if (aps[i].strength > aps[j].strength)
                aps[j] = aps[i];
            continue;
        }
        aps[k++] = aps[i];
    }
    for (int i = 1; i < k; i++) { /* strongest first */
        hm_wifi_ap a = aps[i];
        int j = i;

        while (j > 0 && aps[j - 1].strength < a.strength) {
            aps[j] = aps[j - 1];
            j--;
        }
        aps[j] = a;
    }
    return k;
}

int hm_wifi_secure(int security)
{
    return security == HM_WIFI_SEC_NONE       ? 0
           : security == HM_WIFI_SEC_WPA_TKIP ? 3
           : security == HM_WIFI_SEC_WPA_AES  ? 4
                                              : -1;
}

static int is_hex(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int hm_wifi_key_ok(const char *key, int *hex)
{
    int n = (int)strlen(key), all_hex = 1;

    for (int i = 0; i < n; i++) {
        if (key[i] < 0x20 || key[i] > 0x7E) /* (the keyboard's Cyrillic is not a WPA key) */
            return 0;
        all_hex &= is_hex(key[i]);
    }
    *hex = n == 64 && all_hex;
    return (n >= 8 && n <= 63) || *hex;
}

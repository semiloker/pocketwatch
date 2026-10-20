// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "pwtest.h"
#include "wifi_list.h"

PW_TEST(wifi_list_drops_hidden_keeps_the_strongest_of_a_name_strongest_first)
{
    hm_wifi_ap a[5] = {
        {"Home", 40, HM_WIFI_SEC_WPA_AES, 1}, {"", 90, HM_WIFI_SEC_NONE, 6},
        {"Cafe", 70, HM_WIFI_SEC_NONE, 11},   {"Home", 80, HM_WIFI_SEC_WPA_AES, 6},
        {"Old", 20, HM_WIFI_SEC_WEP, 3},
    };

    CHECK_EQ(hm_wifi_tidy(a, 5), 3);
    CHECK_STR(a[0].ssid, "Home");
    CHECK_EQ(a[0].strength, 80);
    CHECK_EQ(a[0].channel, 6);
    CHECK_STR(a[1].ssid, "Cafe");
    CHECK_STR(a[2].ssid, "Old");
    CHECK_EQ(hm_wifi_tidy(a, 0), 0);
}

PW_TEST(wifi_connection_security_and_keys)
{
    int hex = -1;

    CHECK_EQ(hm_wifi_secure(HM_WIFI_SEC_NONE), 0);
    CHECK_EQ(hm_wifi_secure(HM_WIFI_SEC_WPA_TKIP), 3);
    CHECK_EQ(hm_wifi_secure(HM_WIFI_SEC_WPA_AES), 4);
    CHECK_EQ(hm_wifi_secure(HM_WIFI_SEC_WEP), -1); /* the XMB's Network Settings */
    CHECK_EQ(hm_wifi_secure(HM_WIFI_SEC_UNSUPPORTED), -1);
    CHECK(hm_wifi_key_ok("12345678", &hex));
    CHECK_EQ(hex, 0);
    CHECK(!hm_wifi_key_ok("1234567", &hex));   /* too short */
    CHECK(!hm_wifi_key_ok("пароль123", &hex)); /* not ASCII */
    CHECK(hm_wifi_key_ok("0123456789abcdef0123456789ABCDEF0123456789abcdef0123456789abcdef", &hex));
    CHECK_EQ(hex, 1); /* 64 hex digits: the key itself */
    CHECK(
        !hm_wifi_key_ok("0123456789abcdef0123456789ABCDEF0123456789abcdef0123456789abcdeg", &hex));
}

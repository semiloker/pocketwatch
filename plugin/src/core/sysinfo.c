// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspkernel.h>
#include <psppower.h>
#include <pspreg.h>
#include <psprtc.h>
#include <string.h>

#include "core/log.h"
#include "core/sysinfo.h"
#include "stb_sprintf.h"

/* core/stubs/scePower_driver.S */
int scePowerIsBatteryExist(void);

static pw_sysinfo g_info = {1, 0, 0, 0};

static int read_key(REGHANDLE cat, const char *name, int *out)
{
    REGHANDLE key;
    unsigned int type = 0, value = 0;
    SceSize size = 0;

    if (sceRegGetKeyInfo(cat, name, &key, &type, &size) < 0 || size != 4)
        return -1;
    if (sceRegGetKeyValue(cat, key, &value, 4) < 0)
        return -1;
    *out = (int)value;
    return 0;
}

static int read_category(REGHANDLE reg, const char *path, const char *k1, int *v1, const char *k2,
                         int *v2)
{
    REGHANDLE cat;
    int ok = 0;

    if (sceRegOpenCategory(reg, path, 2, &cat) < 0)
        return 0;
    ok += read_key(cat, k1, v1) == 0;
    if (k2)
        ok += read_key(cat, k2, v2) == 0;
    sceRegCloseCategory(cat);
    return ok;
}

void pw_sysinfo_init(void)
{
    struct RegParam param;
    REGHANDLE reg;
    int found = 0, dummy = 0;

    memset(&param, 0, sizeof(param));
    param.regtype = 1;
    strcpy(param.name, "/system");
    param.namelen = strlen(param.name);
    param.unk2 = 1;
    param.unk3 = 1;
    if (sceRegOpenRegistry(&param, 2, &reg) < 0) { /* 2: read only */
        pw_log("sysinfo: registry not readable: default formats");
        return;
    }
    found += read_category(reg, "/CONFIG/DATE", "date_format", &g_info.date_format, "time_format",
                           &g_info.time_format);
    found +=
        read_category(reg, "/CONFIG/SYSTEM/XMB/THEME", "color_mode", &g_info.color_mode, 0, &dummy);
    sceRegCloseRegistry(reg);
    g_info.read_ok = found;
    pw_log("sysinfo: date format %d, time format %d, theme colour %d (%d key(s) read)",
           g_info.date_format, g_info.time_format, g_info.color_mode, found);
}

const pw_sysinfo *pw_sysinfo_get(void)
{
    return &g_info;
}

int pw_sysinfo_clock(char *out, unsigned int cap)
{
    ScePspDateTime t;
    int hour;

    if (sceRtcGetCurrentClockLocalTime(&t) < 0) {
        out[0] = 0;
        return 1;
    }
    hour = t.hour;
    char date[8];

    if (g_info.date_format == 2) /* day first */
        stbsp_snprintf(date, sizeof(date), "%d/%d", t.day, t.month);
    else
        stbsp_snprintf(date, sizeof(date), "%d/%d", t.month, t.day);
    if (g_info.time_format == 1)
        stbsp_snprintf(out, (int)cap, "%s %d:%02d %s", date, hour % 12 ? hour % 12 : 12, t.minute,
                       hour < 12 ? "AM" : "PM");
    else
        stbsp_snprintf(out, (int)cap, "%s %d:%02d", date, hour, t.minute);
    return t.month >= 1 && t.month <= 12 ? t.month : 1;
}

int pw_sysinfo_battery(int *charging)
{
    int pct;

    *charging = 0;
    if (scePowerIsBatteryExist() <= 0)
        return -1;
    pct = scePowerGetBatteryLifePercent();
    if (pct < 0)
        return -1;
    *charging = scePowerIsPowerOnline() > 0 && scePowerIsBatteryCharging() > 0;
    return pct > 100 ? 100 : pct;
}

int pw_sysinfo_sky_index(int month)
{
    if (g_info.color_mode >= 1 && g_info.color_mode <= 12)
        return g_info.color_mode - 1;
    return month >= 1 && month <= 12 ? month - 1 : 0;
}

void pw_sysinfo_format_date(const pw_datetime *t, char *out, unsigned int cap)
{
    int hour = t->hour;
    char tm[16];

    if (g_info.time_format == 1)
        stbsp_snprintf(tm, sizeof(tm), "%d:%02d %s", hour % 12 ? hour % 12 : 12, t->minute,
                       hour < 12 ? "AM" : "PM");
    else
        stbsp_snprintf(tm, sizeof(tm), "%d:%02d", hour, t->minute);
    if (g_info.date_format == 1)
        stbsp_snprintf(out, (int)cap, "%02d/%02d/%04d %s", t->month, t->day, t->year, tm);
    else if (g_info.date_format == 2)
        stbsp_snprintf(out, (int)cap, "%02d/%02d/%04d %s", t->day, t->month, t->year, tm);
    else
        stbsp_snprintf(out, (int)cap, "%04d/%02d/%02d %s", t->year, t->month, t->day, tm);
}

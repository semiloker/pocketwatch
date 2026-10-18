// SPDX-License-Identifier: GPL-3.0-or-later
#include <malloc.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <psppower.h>
#include <psprtc.h>
#include <psputility_sysparam.h>
#include <string.h>

#include "hw.h"
#include "stb_sprintf.h"

#define STRIDE 512
#define FRAME  (STRIDE * 272)

static unsigned int *g_frame[2];
static int g_back;
static pw_canvas g_canvas;
static volatile int g_exit;

static int exit_cb(int a, int b, void *c)
{
    (void)a;
    (void)b;
    (void)c;
    g_exit = 1;
    return 0;
}

static int callback_thread(SceSize args, void *argp)
{
    int cb = sceKernelCreateCallback("PocketwatchHomeExit", exit_cb, NULL);

    (void)args;
    (void)argp;
    sceKernelRegisterExitCallback(cb);
    sceKernelSleepThreadCB();
    return 0;
}

int hm_hw_init(void)
{
    SceUID th = sceKernelCreateThread("PocketwatchHomeCb", callback_thread, 0x11, 0x1000, 0, 0);

    if (th >= 0)
        sceKernelStartThread(th, 0, 0);
    for (int i = 0; i < 2; i++) {
        g_frame[i] = memalign(64, FRAME * 4);
        if (!g_frame[i])
            return -1;
        memset(g_frame[i], 0, FRAME * 4);
    }
    sceKernelDcacheWritebackAll();
    sceDisplaySetMode(0, 480, 272);
    sceDisplaySetFrameBuf(g_frame[0], STRIDE, PSP_DISPLAY_PIXEL_FORMAT_8888,
                          PSP_DISPLAY_SETBUF_NEXTFRAME);
    g_back = 1;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    return 0;
}

pw_canvas *hm_hw_begin(void)
{
    g_canvas.px = g_frame[g_back];
    g_canvas.w = 480;
    g_canvas.h = 272;
    g_canvas.stride = STRIDE;
    return &g_canvas;
}

void hm_hw_present(void)
{
    sceKernelDcacheWritebackRange(g_frame[g_back], FRAME * 4); /* the display reads memory */
    sceDisplayWaitVblankStart();
    sceDisplaySetFrameBuf(g_frame[g_back], STRIDE, PSP_DISPLAY_PIXEL_FORMAT_8888,
                          PSP_DISPLAY_SETBUF_IMMEDIATE);
    g_back ^= 1;
}

unsigned int hm_hw_buttons(void)
{
    SceCtrlData pad;

    return sceCtrlPeekBufferPositive(&pad, 1) < 0 ? 0 : pad.Buttons;
}

unsigned int hm_hw_now_ms(void)
{
    return sceKernelGetSystemTimeLow() / 1000;
}

int hm_hw_exit_requested(void)
{
    return g_exit;
}

void hm_hw_clock(char *out, int cap)
{
    ScePspDateTime t;
    int datef = 0, timef = 0;
    char tm[16];

    if (sceRtcGetCurrentClockLocalTime(&t) < 0) {
        stbsp_snprintf(out, cap, "--:--");
        return;
    }
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_DATE_FORMAT, &datef);
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_TIME_FORMAT, &timef);
    if (timef == PSP_SYSTEMPARAM_TIME_FORMAT_12HR)
        stbsp_snprintf(tm, sizeof(tm), "%d:%02d %s", t.hour % 12 ? t.hour % 12 : 12, t.minute,
                       t.hour < 12 ? "AM" : "PM");
    else
        stbsp_snprintf(tm, sizeof(tm), "%d:%02d", t.hour, t.minute);
    if (datef == PSP_SYSTEMPARAM_DATE_FORMAT_DDMMYYYY)
        stbsp_snprintf(out, cap, "%d/%d %s", t.day, t.month, tm);
    else
        stbsp_snprintf(out, cap, "%d/%d %s", t.month, t.day, tm);
}

int hm_hw_battery(int *charging)
{
    int p;

    *charging = scePowerIsBatteryCharging() == 1;
    if (scePowerIsBatteryExist() != 1)
        return -1;
    p = scePowerGetBatteryLifePercent();
    return p < 0 || p > 100 ? -1 : p;
}

int hm_hw_cross_enter(void)
{
    int v = 1;

    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_UNKNOWN, &v); /* 9: the confirm button */
    return v != 0;
}

int hm_hw_language(void)
{
    int lang = PSP_SYSTEMPARAM_LANGUAGE_ENGLISH;

    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &lang);
    return lang;
}

unsigned int hm_hw_free_mb(void)
{
    /* sceIoDevctl 0x02425818 fills a pointer to: max clusters, free clusters, max sectors,
       sector size, sectors per cluster (PSPSDK samples, used by every file manager) */
    unsigned int info[5] = {0, 0, 0, 0, 0}, *p = info;

    if (sceIoDevctl("ms0:", 0x02425818, &p, sizeof(p), NULL, 0) < 0)
        return 0;
    return (unsigned int)((unsigned long long)info[1] * info[3] * info[4] / (1024 * 1024));
}

int hm_hw_read_file(const char *path, void *buf, unsigned int cap)
{
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    int n;

    if (fd < 0)
        return fd;
    n = sceIoRead(fd, buf, cap);
    sceIoClose(fd);
    return n;
}

void hm_hw_touch(const char *path)
{
    SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    u64 now = 0;
    char text[24];

    if (fd >= 0) {
        sceRtcGetCurrentTick(&now);
        stbsp_snprintf(text, sizeof(text), "%llu", (unsigned long long)now);
        sceIoWrite(fd, text, strlen(text));
        sceIoClose(fd);
    }
}

void hm_hw_remove(const char *path)
{
    sceIoRemove(path);
}

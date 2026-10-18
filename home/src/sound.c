// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspaudio.h>
#include <pspkernel.h>

#include "sound.h"

static short g_pcm[HM_SNDS][HM_SND_MAX] __attribute__((aligned(64)));
static int g_len[HM_SNDS];
static int g_ch = -1;
static SceUID g_sema = -1, g_thread = -1;
static volatile int g_want = -1, g_quit, g_on = 1;

static int player(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    while (!g_quit) {
        int s;

        sceKernelWaitSema(g_sema, 1, 0);
        while (!g_quit && (s = g_want) >= 0) { /* the latest asked for */
            g_want = -1;
            for (int i = 0; i < g_len[s] && g_want < 0 && !g_quit; i += HM_SND_CHUNK)
                sceAudioOutputBlocking(g_ch, PSP_AUDIO_VOLUME_MAX, g_pcm[s] + i);
        }
    }
    return 0;
}

void hm_sound_init(void)
{
    if (g_thread >= 0)
        return;
    for (int s = 0; s < HM_SNDS; s++)
        g_len[s] = hm_sound_make(s, g_pcm[s], HM_SND_MAX);
    sceKernelDcacheWritebackRange(g_pcm, sizeof(g_pcm)); /* the audio DMA reads memory */
    g_ch = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, HM_SND_CHUNK, PSP_AUDIO_FORMAT_MONO);
    if (g_ch < 0)
        return; /* no sounds: everything else goes on */
    g_quit = 0;
    g_want = -1;
    g_sema = sceKernelCreateSema("PocketwatchHomeSound", 0, 0, 1, 0);
    g_thread = g_sema >= 0 ? sceKernelCreateThread("PocketwatchHomeSound", player, 0x16, 0x1000,
                                                   0, 0)
                           : -1;
    if (g_thread < 0 || sceKernelStartThread(g_thread, 0, 0) < 0) {
        hm_sound_end();
        return;
    }
}

void hm_sound_end(void)
{
    SceUInt wait = 200 * 1000;

    g_quit = 1;
    if (g_thread >= 0) {
        sceKernelSignalSema(g_sema, 1);
        if (sceKernelWaitThreadEnd(g_thread, &wait) < 0)
            sceKernelTerminateThread(g_thread);
        sceKernelDeleteThread(g_thread);
        g_thread = -1;
    }
    if (g_sema >= 0)
        sceKernelDeleteSema(g_sema);
    g_sema = -1;
    if (g_ch >= 0)
        sceAudioChRelease(g_ch);
    g_ch = -1;
}

void hm_sound_enable(int on)
{
    g_on = on;
}

void hm_sound_play(int s)
{
    if (!g_on || g_thread < 0 || s < 0 || s >= HM_SNDS || g_len[s] <= 0)
        return;
    g_want = s;
    sceKernelSignalSema(g_sema, 1); /* (already signalled: the thread takes the latest) */
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include <malloc.h>
#include <pspaudio.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <pspmp3.h>
#include <psputility.h>
#include <stdlib.h>
#include <string.h>

#include "music.h"
#include "stb_sprintf.h"

#define MUSIC_DIR "ms0:/MUSIC"
#define STREAM    (16 * 1024) /* MP3 data handed to the decoder at a time */
#define PCM       (16 * 1152) /* bytes of decoded samples (1152 stereo 16-bit samples, x4) */

static hm_track *g_tracks;
static int g_count;

/* ---- the list ---- */

static int is_mp3(const char *name)
{
    int n = (int)strlen(name);

    return n > 4 && name[n - 4] == '.' && (name[n - 3] | 32) == 'm' && (name[n - 2] | 32) == 'p' &&
           name[n - 1] == '3';
}

static void scan(const char *dir, int depth)
{
    SceUID dd = sceIoDopen(dir);
    SceIoDirent e;

    if (dd < 0)
        return;
    memset(&e, 0, sizeof(e));
    while (sceIoDread(dd, &e) > 0 && g_count < HM_TRACKS_MAX) {
        char path[192];

        if (e.d_name[0] != '.') {
            stbsp_snprintf(path, sizeof(path), "%s/%s", dir, e.d_name);
            if (FIO_S_ISDIR(e.d_stat.st_mode)) {
                if (depth == 0)
                    scan(path, 1);
            } else if (is_mp3(e.d_name)) {
                hm_track *t = &g_tracks[g_count++];

                stbsp_snprintf(t->path, sizeof(t->path), "%s", path);
                hm_music_name(e.d_name, t->title, sizeof(t->title), t->artist, sizeof(t->artist));
            }
        }
        memset(&e, 0, sizeof(e));
    }
    sceIoDclose(dd);
}

static int by_title(const void *a, const void *b)
{
    return strcmp(((const hm_track *)a)->title, ((const hm_track *)b)->title);
}

int hm_music_scan(void)
{
    if (!g_tracks && !(g_tracks = malloc(sizeof(hm_track) * HM_TRACKS_MAX)))
        return 0;
    g_count = 0;
    scan(MUSIC_DIR, 0);
    qsort(g_tracks, (size_t)g_count, sizeof(hm_track), by_title);
    return g_count;
}

int hm_music_count(void)
{
    return g_count;
}

const hm_track *hm_music_track(int i)
{
    return i >= 0 && i < g_count ? &g_tracks[i] : 0;
}

/* ---- the player thread ---- */

static volatile int g_want = -1; /* a track asked for */
static volatile int g_playing = -1;
static volatile int g_paused, g_stop;
static volatile unsigned int g_samples, g_rate, g_frames;
static int g_ready; /* the decoder's modules loaded */
static SceUID g_thread = -1;

static int feed(SceInt32 h, SceUID fd)
{
    SceUChar8 *dst;
    SceInt32 want, pos;
    int n;

    if (sceMp3GetInfoToAddStreamData(h, &dst, &want, &pos) < 0)
        return -1;
    sceIoLseek32(fd, pos, PSP_SEEK_SET);
    n = sceIoRead(fd, dst, (unsigned int)want);
    if (n < 0)
        return -1;
    return sceMp3NotifyAddStreamData(h, n) < 0 ? -1 : n;
}

/* The output channel is busy until its last samples have played: a release or a reserve right
   after one fails for a moment (the next track did not start), so both wait for it. */
static int reserve(int samples, int rate, int ch)
{
    for (int i = 0; i < 50; i++) {
        if (sceAudioSRCChReserve(samples, rate, ch) >= 0)
            return 0;
        sceKernelDelayThread(10 * 1000);
    }
    return -1;
}

static void release(void)
{
    for (int i = 0; i < 50 && sceAudioSRCChRelease() < 0; i++)
        sceKernelDelayThread(10 * 1000);
}

/* One file, to its end or until something else is asked for: 1 at its end, 0 when stopped or
   another track was asked for, -1 when it could not be played. */
static int play(const char *path)
{
    static SceUChar8 __attribute__((aligned(64))) stream[STREAM];
    static SceShort16 __attribute__((aligned(64))) pcm[PCM / 2];
    unsigned char head[10];
    SceMp3InitArg a;
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    SceInt32 h;
    int size, end = 0, ch;

    if (fd < 0)
        return -1;
    size = (int)sceIoLseek32(fd, 0, PSP_SEEK_END);
    sceIoLseek32(fd, 0, PSP_SEEK_SET);
    memset(head, 0, sizeof(head));
    sceIoRead(fd, head, sizeof(head));
    memset(&a, 0, sizeof(a));
    a.mp3StreamStart = hm_music_id3_size(head); /* the decoder starts at the first frame */
    a.mp3StreamEnd = size;
    a.mp3Buf = stream;
    a.mp3BufSize = STREAM;
    a.pcmBuf = (SceUChar8 *)pcm;
    a.pcmBufSize = PCM;
    if ((h = sceMp3ReserveMp3Handle(&a)) < 0) {
        sceIoClose(fd);
        return -1;
    }
    if (feed(h, fd) < 0 || sceMp3Init(h) < 0) {
        sceMp3ReleaseMp3Handle(h);
        sceIoClose(fd);
        return -1;
    }
    sceMp3SetLoopNum(h, 0); /* once: the decoder would start the file again at its end */
    g_rate = (unsigned int)sceMp3GetSamplingRate(h);
    g_frames = (unsigned int)sceMp3GetFrameNum(h);
    g_samples = 0;
    ch = sceMp3GetMp3ChannelNum(h) == 1 ? 1 : 2;
    if (g_rate == 0 || reserve(sceMp3GetMaxOutputSample(h), (int)g_rate, ch) < 0) {
        sceMp3ReleaseMp3Handle(h);
        sceIoClose(fd);
        return -1;
    }
    while (!g_stop && g_want < 0) {
        SceShort16 *out;
        int bytes;

        if (g_paused) {
            sceKernelDelayThread(20 * 1000);
            continue;
        }
        if (sceMp3CheckStreamDataNeeded(h) > 0 && feed(h, fd) < 0)
            break;
        bytes = sceMp3Decode(h, &out);
        if (bytes <= 0) { /* the end of the file; a broken one plays nothing */
            end = g_samples ? 1 : -1;
            break;
        }
        g_samples += (unsigned int)bytes / (2u * (unsigned int)ch);
        sceAudioSRCOutputBlocking(PSP_AUDIO_VOLUME_MAX, out);
    }
    release();
    sceMp3ReleaseMp3Handle(h);
    sceIoClose(fd);
    return end;
}

static int player(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    for (;;) {
        int i = g_want;

        if (i < 0) {
            sceKernelDelayThread(20 * 1000);
            continue;
        }
        g_want = -1;
        g_stop = 0;
        g_paused = 0;
        for (int failed = 0; i >= 0 && i < g_count && failed < 3;) {
            int r;

            g_playing = i;
            r = play(g_tracks[i].path);
            if (r == 0 || g_stop || g_want >= 0)
                break;
            failed = r < 0 ? failed + 1 : 0; /* three files that do not play: stop */
            i = (i + 1) % g_count;           /* the next one follows */
        }
        if (g_want < 0)
            g_playing = -1;
    }
    return 0;
}

void hm_music_play(int i)
{
    if (i < 0 || i >= g_count)
        return;
    if (!g_ready) {
        sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
        sceUtilityLoadModule(PSP_MODULE_AV_MP3);
        g_ready = sceMp3InitResource() >= 0 ? 1 : -1;
    }
    if (g_ready < 0)
        return;
    if (g_thread < 0) {
        /* above the screen's thread: the audio must not wait for a frame */
        g_thread = sceKernelCreateThread("PocketwatchHomeMusic", player, 0x18, 0x4000, 0, 0);
        if (g_thread >= 0)
            sceKernelStartThread(g_thread, 0, 0);
    }
    g_want = i;
}

void hm_music_toggle(void)
{
    if (g_playing >= 0)
        g_paused = !g_paused;
}

void hm_music_stop(void)
{
    g_stop = 1;
    g_paused = 0;
    for (int i = 0; i < 50 && g_playing >= 0; i++) /* the thread lets the file go (USB) */
        sceKernelDelayThread(10 * 1000);
}

void hm_music_skip(int dir)
{
    if (g_playing >= 0 && g_count > 0)
        hm_music_play((g_playing + dir + g_count) % g_count);
}

int hm_music_current(void)
{
    return g_playing;
}

int hm_music_paused(void)
{
    return g_paused;
}

void hm_music_time(unsigned int *played, unsigned int *length)
{
    unsigned int rate = g_rate;

    *played = rate ? g_samples / rate : 0;
    *length = rate ? (unsigned int)((unsigned long long)g_frames * 1152 / rate) : 0;
}

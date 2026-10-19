// SPDX-License-Identifier: GPL-3.0-or-later
#include <malloc.h>
#include <pspaudio.h>
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <pspmp3.h>
#include <psputility.h>
#include <stdlib.h>
#include <string.h>

#include "hcfg.h"
#include "music.h"
#include "stb_sprintf.h"

#define MUSIC_DIR "ms0:/MUSIC"
#define STREAM    (16 * 1024) /* MP3 data handed to the decoder at a time */
#define PCM       (16 * 1152) /* bytes of decoded samples (1152 stereo 16-bit samples, x4) */
#define SYNC_READ 4096        /* bytes searched for a frame after a seek */

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

                memset(t, 0, sizeof(*t));
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

/* ---- the tags: a thread of its own, after the list is made ---- */

static SceUID g_tags_thread = -1;
static volatile int g_tags_pause, g_tags_idle;

static int read_fd(void *ctx, unsigned int off, void *buf, unsigned int n)
{
    SceUID fd = (SceUID)(long)ctx;

    if (sceIoLseek32(fd, (int)off, PSP_SEEK_SET) < 0)
        return -1;
    return sceIoRead(fd, buf, n);
}

static int tagger(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    for (int i = 0; i < g_count; i++) {
        hm_track *t = &g_tracks[i];
        SceUID fd;
        hm_id3 tag;

        while (g_tags_pause) {
            g_tags_idle = 1;
            sceKernelDelayThread(50 * 1000);
        }
        g_tags_idle = 0;
        if (t->tagged)
            continue;
        if ((fd = sceIoOpen(t->path, PSP_O_RDONLY, 0)) >= 0) {
            if (hm_id3_read(read_fd, (void *)(long)fd, &tag) == 0) {
                /* the tag's words over the file name's (a file name can be wrong) */
                if (tag.title[0])
                    stbsp_snprintf(t->title, sizeof(t->title), "%s", tag.title);
                if (tag.artist[0])
                    stbsp_snprintf(t->artist, sizeof(t->artist), "%s", tag.artist);
                if (tag.album[0])
                    stbsp_snprintf(t->album, sizeof(t->album), "%s", tag.album);
                t->pic_off = tag.pic_off;
                t->pic_len = tag.pic_len;
            }
            sceIoClose(fd);
        }
        t->tagged = 1;
        sceKernelDelayThread(2 * 1000); /* (the screen's and the player's reads first) */
    }
    g_tags_idle = 1;
    g_tags_thread = -1;
    sceKernelExitDeleteThread(0);
    return 0;
}

int hm_music_scan(void)
{
    if (!g_tracks && !(g_tracks = malloc(sizeof(hm_track) * HM_TRACKS_MAX)))
        return 0;
    hm_music_pause_tags(1);
    g_count = 0;
    scan(MUSIC_DIR, 0);
    qsort(g_tracks, (size_t)g_count, sizeof(hm_track), by_title); /* (by the file names) */
    hm_music_pause_tags(0);
    if (g_count > 0 && g_tags_thread < 0) {
        g_tags_thread = sceKernelCreateThread("PocketwatchHomeTags", tagger, 0x3A, 0x2000, 0, 0);
        if (g_tags_thread >= 0)
            sceKernelStartThread(g_tags_thread, 0, 0);
    }
    return g_count;
}

void hm_music_pause_tags(int on)
{
    g_tags_pause = on;
    if (!on || g_tags_thread < 0)
        return;
    g_tags_idle = 0;
    for (int i = 0; i < 100 && !g_tags_idle && g_tags_thread >= 0; i++)
        sceKernelDelayThread(10 * 1000);
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
static volatile int g_seek = -1; /* a time asked for (s) */
static volatile unsigned int g_samples, g_rate, g_frames;
static volatile int g_shuffle, g_repeat = HM_REPEAT_ALL;
static int g_ready; /* the decoder's modules loaded */
static SceUID g_thread = -1;
static unsigned int g_rand = 0x9E3779B9u;
static int g_history[16], g_nhist; /* shuffle: where back goes */

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

/* A decoder over the file from byte start (a frame's first byte) to its end; < 0 on an error. */
static SceInt32 open_at(SceUID fd, int start, int size, SceUChar8 *stream, SceShort16 *pcm)
{
    SceMp3InitArg a;
    SceInt32 h;

    memset(&a, 0, sizeof(a));
    a.mp3StreamStart = (unsigned int)start;
    a.mp3StreamEnd = (unsigned int)size;
    a.mp3Buf = stream;
    a.mp3BufSize = STREAM;
    a.pcmBuf = (SceUChar8 *)pcm;
    a.pcmBufSize = PCM;
    if ((h = sceMp3ReserveMp3Handle(&a)) < 0)
        return -1;
    if (feed(h, fd) < 0 || sceMp3Init(h) < 0) {
        sceMp3ReleaseMp3Handle(h);
        return -1;
    }
    sceMp3SetLoopNum(h, 0); /* once: the decoder would start the file again at its end */
    return h;
}

/* The byte of the frame nearest to second s (by the file's average rate: exact for a constant
   bit rate), or -1. */
static int seek_byte(SceUID fd, int data, int size, unsigned int ref, unsigned int s,
                     unsigned int length)
{
    static unsigned char buf[SYNC_READ];
    long long at;
    int n, k;

    if (length == 0)
        return -1;
    at = data + (long long)(size - data) * s / length;
    if (at < data)
        at = data;
    if (at >= size - 4)
        return -1;
    if (sceIoLseek32(fd, (int)at, PSP_SEEK_SET) < 0 || (n = sceIoRead(fd, buf, SYNC_READ)) < 8)
        return -1;
    k = hm_mp3_find_frame(buf, n, ref);
    return k < 0 ? -1 : (int)at + k;
}

/* One file, to its end or until something else is asked for: 1 at its end, 0 when stopped or
   another track was asked for, -1 when it could not be played. */
static int play(const char *path)
{
    static SceUChar8 __attribute__((aligned(64))) stream[STREAM];
    static SceShort16 __attribute__((aligned(64))) pcm[PCM / 2];
    unsigned char head[10], first[4];
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    SceInt32 h;
    int size, end = 0, ch, data, reserved = 0;
    unsigned int ref = 0;

    if (fd < 0)
        return -1;
    size = (int)sceIoLseek32(fd, 0, PSP_SEEK_END);
    sceIoLseek32(fd, 0, PSP_SEEK_SET);
    memset(head, 0, sizeof(head));
    sceIoRead(fd, head, sizeof(head));
    data = (int)hm_music_id3_size(head); /* the decoder starts at the first frame */
    if (sceIoLseek32(fd, data, PSP_SEEK_SET) >= 0 && sceIoRead(fd, first, 4) == 4)
        ref = (unsigned int)first[0] << 24 | (unsigned int)first[1] << 16 |
              (unsigned int)first[2] << 8 | first[3];
    g_samples = 0;
    g_seek = -1;
    if ((h = open_at(fd, data, size, stream, pcm)) < 0) {
        sceIoClose(fd);
        return -1;
    }
    g_rate = (unsigned int)sceMp3GetSamplingRate(h);
    g_frames = (unsigned int)sceMp3GetFrameNum(h);
    ch = sceMp3GetMp3ChannelNum(h) == 1 ? 1 : 2;
    if (g_rate == 0 || reserve(sceMp3GetMaxOutputSample(h), (int)g_rate, ch) < 0) {
        sceMp3ReleaseMp3Handle(h);
        sceIoClose(fd);
        return -1;
    }
    reserved = 1;
    while (!g_stop && g_want < 0) {
        SceShort16 *out;
        int bytes;

        if (g_seek >= 0) { /* a time asked for: the decoder again from the frame there */
            unsigned int s = (unsigned int)g_seek,
                         length = g_rate
                                      ? (unsigned int)((unsigned long long)g_frames * 1152 / g_rate)
                                      : 0;
            int at = s == 0 ? data : seek_byte(fd, data, size, ref, s, length);
            SceInt32 h2;

            g_seek = -1;
            if (at >= 0 && (h2 = open_at(fd, at, size, stream, pcm)) >= 0) {
                sceMp3ReleaseMp3Handle(h);
                h = h2;
                g_samples = s * g_rate;
            } else if (at >= 0) { /* (the old one stays) */
                continue;
            } else if (s > 0) { /* past the end: the next one */
                end = 1;
                break;
            }
            continue;
        }
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
    if (reserved)
        release();
    sceMp3ReleaseMp3Handle(h);
    sceIoClose(fd);
    return end;
}

static int next_random(int i)
{
    int k;

    if (g_count <= 1)
        return 0;
    g_rand = g_rand * 1664525u + 1013904223u + (unsigned int)sceKernelGetSystemTimeLow();
    k = (int)((g_rand >> 8) % (unsigned int)(g_count - 1));
    return k >= i ? k + 1 : k; /* never the same one again */
}

static void remember(int i)
{
    if (g_nhist == 16) {
        memmove(g_history, g_history + 1, sizeof(int) * 15);
        g_nhist--;
    }
    g_history[g_nhist++] = i;
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
        for (int failed = 0, played = 0; i >= 0 && i < g_count && failed < 3;) {
            int r;

            g_playing = i;
            r = play(g_tracks[i].path);
            if (r == 0 || g_stop || g_want >= 0)
                break;
            failed = r < 0 ? failed + 1 : 0; /* three files that do not play: stop */
            played++;
            remember(i);
            if (g_repeat == HM_REPEAT_ONE && r > 0)
                continue; /* the same one again */
            if (g_shuffle) {
                if (g_repeat == HM_REPEAT_OFF && played >= g_count)
                    break;
                i = next_random(i);
            } else if (i + 1 < g_count) {
                i++;
            } else if (g_repeat == HM_REPEAT_ALL) {
                i = 0; /* round again */
            } else {
                break;
            }
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
    if (g_playing >= 0 && g_playing != i)
        remember(g_playing);
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
    unsigned int played, length;
    int i = g_playing;

    if (i < 0 || g_count <= 0)
        return;
    hm_music_time(&played, &length);
    if (dir < 0 && played > 3) { /* back: this one from its start */
        g_seek = 0;
        return;
    }
    if (dir < 0 && g_shuffle && g_nhist > 0) {
        int back = g_history[--g_nhist];

        g_want = back; /* (not remembered again) */
        return;
    }
    hm_music_play(dir > 0 && g_shuffle ? next_random(i) : (i + dir + g_count) % g_count);
}

void hm_music_seek(int seconds)
{
    unsigned int played, length;
    int to;

    if (g_playing < 0)
        return;
    hm_music_time(&played, &length);
    to = (g_seek >= 0 ? g_seek : (int)played) + seconds; /* (one asked for and not done yet) */
    if (to < 0)
        to = 0;
    if (length > 0 && to >= (int)length)
        to = (int)length - 1;
    g_seek = to;
}

void hm_music_mode(int shuffle, int repeat)
{
    g_shuffle = shuffle;
    g_repeat = repeat;
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

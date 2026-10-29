// SPDX-License-Identifier: GPL-3.0-or-later
/* Home's key sounds, made from a few numbers each (portable, tests/test_sound.c). */
#include "sound.h"

typedef struct {
    float f0, f1;  /* Hz at the start and the end (a glide between them) */
    float partial; /* a second tone at this multiple of the first, half as loud; 0: none */
    float amp;     /* the peak, of full scale */
    int ms, attack_ms, tau_ms; /* length; rise; how fast it dies away (e^-1) */
} tone;

static const tone k_tones[HM_SNDS] = {
    [HM_SND_MOVE] = {2200, 2200, 0, 0.10f, 28, 1, 5},
    [HM_SND_TAB] = {1500, 1500, 2.0f, 0.11f, 45, 1, 9},
    [HM_SND_OPEN] = {880, 1320, 1.5f, 0.13f, 80, 1, 18},
    [HM_SND_BACK] = {1320, 880, 1.5f, 0.12f, 80, 1, 18},
    [HM_SND_START] = {660, 1320, 1.5f, 0.14f, 160, 2, 45},
    [HM_SND_ERROR] = {220, 200, 3.0f, 0.12f, 120, 2, 40},
};

/* sin(2 pi p) for a phase p in [0, 1): a parabola, refined once (no libm, -1..1). */
static float wave(float p)
{
    float x = 2.0f * p - 1.0f, y = 4.0f * x * (1.0f - (x < 0 ? -x : x));

    return 0.225f * (y * (y < 0 ? -y : y) - y) + y;
}

int hm_sound_make(int s, short *out, int cap)
{
    const tone *t;
    int n, len, rise, fade = HM_SND_RATE * 2 / 1000; /* the last 2 ms go to silence: no click */
    float p1 = 0, p2 = 0, env = 0, decay;

    if (s < 0 || s >= HM_SNDS)
        return -1;
    t = &k_tones[s];
    n = HM_SND_RATE * t->ms / 1000;
    len = (n + HM_SND_CHUNK - 1) / HM_SND_CHUNK * HM_SND_CHUNK;
    if (len > cap)
        return -1;
    rise = HM_SND_RATE * t->attack_ms / 1000;
    decay = 1.0f - 1000.0f / ((float)t->tau_ms * HM_SND_RATE); /* e^(-1/(tau fs)), nearly */
    for (int i = 0; i < len; i++) {
        float f = t->f0 + (t->f1 - t->f0) * (float)i / (float)n, v;

        if (i >= n) {
            out[i] = 0;
            continue;
        }
        env = i < rise ? (float)(i + 1) / (float)rise : env * decay;
        if (i >= n - fade)
            env = env * (float)(n - i) / (float)(n - i + 1);
        p1 += f / HM_SND_RATE;
        p1 -= (float)(int)p1;
        v = wave(p1);
        if (t->partial > 0) {
            p2 += f * t->partial / HM_SND_RATE;
            p2 -= (float)(int)p2;
            v = (v + 0.5f * wave(p2)) / 1.5f;
        }
        out[i] = (short)(v * env * t->amp * 32767.0f);
    }
    return len;
}

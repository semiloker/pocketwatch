// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspiofilemgr.h>
#include <pspkernel.h>
#include <string.h>

#include "complications/states/savedata_hash.h"
#include "complications/states/sfo.h"
#include "core/log.h"
#include "core/mem.h"
#include "core/platform.h"
#include "core/png.h"
#include "debug/spike_savedata.h"

#define SAVEDATA_DIR "ms0:/PSP/SAVEDATA"
#define ARK_SFO      SAVEDATA_DIR "/ARK_01234/PARAM.SFO"

#define ICON_W 144
#define ICON_H 80
#define PIC_W  480
#define PIC_H  272

typedef struct {
    unsigned int width, height;
    unsigned char accent[3];
} Pattern;

/* Dark radial gradient with a ring, as in tools/spikes/s2_savedata_test.py. Integer math only. */
static void pattern_row(void *ctx, unsigned int y, unsigned char *rgb)
{
    const Pattern *pt = ctx;
    int cx = (int)pt->width / 2, cy = (int)pt->height / 2;
    int radius = (int)(pt->width < pt->height ? pt->width : pt->height) * 36 / 100;
    int span = (int)(pt->width > pt->height ? pt->width : pt->height);
    int ring = radius / 16 + 1;

    for (unsigned int x = 0; x < pt->width; x++) {
        int dx = (int)x - cx, dy = (int)y - cy;
        int d2 = dx * dx + dy * dy;
        int base = 255 - d2 * 255 / (span * span / 4 + 1); /* d2 * 255 < 2^31 for <= 4096 px */

        if (base < 0)
            base = 0;
        if (d2 >= (radius - ring) * (radius - ring) && d2 <= (radius + ring) * (radius + ring)) {
            memcpy(rgb + 3 * x, pt->accent, 3);
        } else {
            rgb[3 * x + 0] = (unsigned char)(20 + 60 * base / 255);
            rgb[3 * x + 1] = (unsigned char)(24 + 70 * base / 255);
            rgb[3 * x + 2] = (unsigned char)(40 + 110 * base / 255);
        }
    }
}

static unsigned int now_ms(void)
{
    return sceKernelGetSystemTimeLow() / 1000;
}

static int read_file(const char *path, unsigned char *buf, unsigned int cap)
{
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    int n;

    if (fd < 0)
        return fd;
    n = sceIoRead(fd, buf, cap);
    sceIoClose(fd);
    return n;
}

static void hex16(char *out, const unsigned char *p)
{
    static const char digits[] = "0123456789abcdef";

    for (int i = 0; i < 16; i++) {
        out[2 * i] = digits[p[i] >> 4];
        out[2 * i + 1] = digits[p[i] & 15];
    }
    out[32] = '\0';
}

/* Step 1: our hashing must reproduce the hash stored in a real, XMB-accepted SFO. */
static void verify_ark_sfo(unsigned char *sfo, unsigned int cap)
{
    unsigned char stored[PW_SFO_PARAMS_SIZE];
    unsigned int off, len;
    char a[33], b[33];
    int size = read_file(ARK_SFO, sfo, cap);

    if (size <= 0 || size % 16 != 0) {
        pw_log("S2b: cannot read %s (0x%08X)", ARK_SFO, size);
        return;
    }
    if (pw_sfo_find(sfo, (unsigned int)size, "SAVEDATA_PARAMS", &off, &len) < 0 ||
        len != PW_SFO_PARAMS_SIZE) {
        pw_log("S2b: SAVEDATA_PARAMS not found in %s", ARK_SFO);
        return;
    }
    memcpy(stored, sfo + off, sizeof(stored));
    if (pw_savedata_hash_sfo(sfo, (unsigned int)size, off) < 0) {
        pw_log("S2b: hashing failed");
        return;
    }

    int match = memcmp(stored, sfo + off, sizeof(stored)) == 0;
    hex16(a, stored + 0x10);
    hex16(b, sfo + off + 0x10);
    pw_log("S2b: ARK_01234 re-hash %s (mode byte %02X/%02X)", match ? "MATCH" : "MISMATCH",
           stored[0], sfo[off]);
    pw_log("S2b:   stored 11C0 %s", a);
    pw_log("S2b:   ours   11C0 %s", b);
    hex16(a, stored + 0x20);
    hex16(b, sfo + off + 0x20);
    pw_log("S2b:   stored 11D0 %s", a);
    pw_log("S2b:   ours   11D0 %s", b);
}

static int write_file(const char *dir, const char *name, const void *data, unsigned int len)
{
    char path[96];

    strcpy(path, dir);
    strcat(path, "/");
    strcat(path, name);
    return pw_file_write(path, data, len, 0);
}

/* Step 2: a complete test folder. */
static void make_folder(const char *name, int list_state_bin, unsigned char *buf, unsigned int cap)
{
    static unsigned char file_list[PW_SFO_FILE_LIST_SIZE];
    static unsigned char state_bin[1024];
    char dir[64];
    unsigned char *sfo = buf;
    unsigned char *scratch = buf + PW_SFO_SAVEDATA_SIZE;
    unsigned char *png = scratch + 1 + 3 * PIC_W + 63;
    unsigned int png_cap = cap - (unsigned int)(png - buf);
    unsigned int params, t0;
    int size, ret;
    Pattern icon = {ICON_W, ICON_H, {120, 200, 230}};
    Pattern pic = {PIC_W, PIC_H, {120, 200, 230}};

    if (list_state_bin) {
        icon.accent[0] = pic.accent[0] = 230;
        icon.accent[1] = pic.accent[1] = 200;
        icon.accent[2] = pic.accent[2] = 120;
    }

    strcpy(dir, SAVEDATA_DIR "/");
    strcat(dir, name);
    ret = sceIoMkdir(dir, 0777);
    pw_log("S2b: %s mkdir 0x%08X", name, ret);

    memset(file_list, 0, sizeof(file_list));
    if (list_state_bin)
        pw_sfo_file_list_entry(file_list, "STATE.BIN");
    pw_savedata_sfo s = {
        .title = "Pocketwatch test",
        .savedata_title = list_state_bin ? "Save State test D" : "Save State test C",
        .detail = list_state_bin ? "Spike S2b, variant D: STATE.BIN in file list.\nHashed (mode 1)."
                                 : "Spike S2b, variant C: empty file list.\nHashed (mode 1).",
        .directory = name,
        .file_list = file_list,
        .parental_level = 1,
    };

    t0 = now_ms();
    size = pw_sfo_build_savedata(&s, sfo, PW_SFO_SAVEDATA_SIZE, &params);
    ret = size < 0 ? -1 : pw_savedata_hash_sfo(sfo, (unsigned int)size, params);
    pw_log("S2b: %s sfo build+hash %d (%u ms)", name, ret, now_ms() - t0);
    if (ret < 0)
        return;
    write_file(dir, "PARAM.SFO", sfo, (unsigned int)size);

    t0 = now_ms();
    size = pw_png_encode_rgb(ICON_W, ICON_H, pattern_row, &icon, scratch, png, png_cap);
    pw_log("S2b: %s ICON0 %d bytes encoded in %u ms", name, size, now_ms() - t0);
    if (size > 0) {
        t0 = now_ms();
        write_file(dir, "ICON0.PNG", png, (unsigned int)size);
        pw_log("S2b: %s ICON0 written in %u ms", name, now_ms() - t0);
    }

    t0 = now_ms();
    size = pw_png_encode_rgb(PIC_W, PIC_H, pattern_row, &pic, scratch, png, png_cap);
    pw_log("S2b: %s PIC1 %d bytes encoded in %u ms", name, size, now_ms() - t0);
    if (size > 0) {
        t0 = now_ms();
        write_file(dir, "PIC1.PNG", png, (unsigned int)size);
        pw_log("S2b: %s PIC1 written in %u ms", name, now_ms() - t0);
    }

    write_file(dir, "STATE.BIN", state_bin, sizeof(state_bin));
    pw_log("S2b: %s done", name);
}

void pw_spike_savedata(void)
{
    pw_mem_block block;
    unsigned int need = PW_SFO_SAVEDATA_SIZE + 1 + 3 * PIC_W + 64 + pw_png_size(PIC_W, PIC_H);
    int ret;

    pw_log("S2b: ---- start ----");
    ret = pw_savedata_hash_init();
    if (ret < 0) {
        pw_log("S2b: chnnlsv unavailable (0x%08X)", ret);
        return;
    }
    ret = pw_mem_alloc(&block, "PwSpikeS2b", need);
    if (ret < 0) {
        pw_log("S2b: alloc %u bytes failed 0x%08X", need, ret);
        return;
    }
    pw_log("S2b: buffer %u bytes at %p", need, block.ptr);

    verify_ark_sfo(block.ptr, PW_SFO_SAVEDATA_SIZE);
    make_folder("PWTEST00C", 0, block.ptr, need);
    make_folder("PWTEST00D", 1, block.ptr, need);

    pw_mem_free(&block);
    pw_log("S2b: ---- end ----");
}

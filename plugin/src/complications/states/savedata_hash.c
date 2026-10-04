// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspkernel.h>
#include <pspmodulemgr.h>
#include <string.h>

#include "complications/states/savedata_hash.h"
#include "complications/states/sfo.h"
#include "core/log.h"
#include "core/sysctrl.h"

#define CHNNLSV_PATH   "flash0:/kd/chnnlsv.prx"
#define CHNNLSV_MODULE "sceChnnlsv"

/* User-exported library: NIDs are not randomized on 6.xx. */
#define NID_SD_SET_INDEX      0xE7833020 /* sceSdSetIndex     = MAC init   */
#define NID_SD_REMOVE_VALUE   0xF21A1FCA /* sceSdRemoveValue  = MAC update */
#define NID_SD_GET_LAST_INDEX 0xC4C494F8 /* sceSdGetLastIndex = MAC final  */

typedef struct {
    int mode;
    unsigned char result[16];
    unsigned char key[16];
    int key_length;
} MacContext;

static int (*g_mac_init)(MacContext *ctx, int mode);
static int (*g_mac_update)(MacContext *ctx, const unsigned char *data, int len);
static int (*g_mac_final)(MacContext *ctx, unsigned char *hash, const unsigned char *key);

static int resolve(const char *module)
{
    g_mac_init = (void *)sctrlHENFindFunction(module, CHNNLSV_MODULE, NID_SD_SET_INDEX);
    g_mac_update = (void *)sctrlHENFindFunction(module, CHNNLSV_MODULE, NID_SD_REMOVE_VALUE);
    g_mac_final = (void *)sctrlHENFindFunction(module, CHNNLSV_MODULE, NID_SD_GET_LAST_INDEX);
    return g_mac_init && g_mac_update && g_mac_final ? 0 : -1;
}

int pw_savedata_hash_init(void)
{
    SceKernelModuleInfo info;
    SceUID mod;
    int ret, status = 0;

    if (resolve(CHNNLSV_MODULE) == 0)
        return 0;

    mod = sceKernelLoadModule(CHNNLSV_PATH, 0, NULL);
    if (mod < 0) {
        pw_log("savedata_hash: load %s failed 0x%08X", CHNNLSV_PATH, mod);
        return mod;
    }
    ret = sceKernelStartModule(mod, 0, NULL, &status, NULL);
    if (ret < 0) {
        pw_log("savedata_hash: start chnnlsv failed 0x%08X", ret);
        return ret;
    }

    /* Resolve by the module's real name rather than trusting CHNNLSV_MODULE. */
    memset(&info, 0, sizeof(info));
    info.size = sizeof(info);
    if (sceKernelQueryModuleInfo(mod, &info) < 0)
        strcpy(info.name, CHNNLSV_MODULE);
    if (resolve(info.name) < 0) {
        pw_log("savedata_hash: MAC functions not found in module '%s'", info.name);
        return -1;
    }
    pw_log("savedata_hash: chnnlsv loaded as '%s'", info.name);
    return 0;
}

/* Data must be padded to 16 bytes, which PW_SFO_SAVEDATA_SIZE is. */
static int mac(unsigned char out[16], const unsigned char *data, unsigned int len, int mode)
{
    MacContext ctx;
    int ret;

    memset(&ctx, 0, sizeof(ctx));
    memset(out, 0, 16);
    if ((ret = g_mac_init(&ctx, mode)) < 0)
        return ret;
    if ((ret = g_mac_update(&ctx, data, (int)len)) < 0)
        return ret;
    return g_mac_final(&ctx, out, NULL);
}

int pw_savedata_hash_sfo(unsigned char *sfo, unsigned int size, unsigned int params_offset)
{
    unsigned char *params = sfo + params_offset;
    unsigned char hash[16];
    int ret;

    if (g_mac_init == NULL || size % 16 != 0 || params_offset + PW_SFO_PARAMS_SIZE > size)
        return -1;

    memset(params, 0, PW_SFO_PARAMS_SIZE);

    /* "11D0" hash over the whole file, then flag protection mode 1. */
    if ((ret = mac(hash, sfo, size, 2)) < 0)
        return ret;
    memcpy(params + 0x20, hash, 16);
    params[0] |= 0x01;

    /* "11C0" hash over the file including the first hash and the flag. */
    if ((ret = mac(hash, sfo, size, 1)) < 0)
        return ret;
    memcpy(params + 0x10, hash, 16);
    return 0;
}

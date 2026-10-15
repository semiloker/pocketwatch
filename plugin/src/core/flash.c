// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspiofilemgr.h>
#include <pspiofilemgr_kernel.h>
#include <pspkernel.h>

#include "core/flash.h"
#include "core/sysctrl.h"

/* IoFileMgrForKernel export whose first JAL is the internal driver lookup (as used by ARK-4). */
#define NID_IOFILEMGR_LOOKUP_HOLDER 0x76DA16E3

static unsigned int set_k1(unsigned int k1)
{
    unsigned int old;

    __asm__ volatile("move %0, $k1\n\tmove $k1, %1" : "=&r"(old) : "r"(k1));
    return old;
}

static unsigned int first_jal_target(unsigned int func)
{
    for (unsigned int a = func; a < func + 0x100; a += 4) {
        unsigned int insn = *(volatile unsigned int *)a;

        if ((insn >> 26) == 3)
            return ((insn & 0x03FFFFFF) << 2) | (a & 0xF0000000);
    }
    return 0;
}

/* A driver list node is {next, drv, arg}: &node[1] is the PspIoDrvArg its functions take. */
static PspIoDrvArg *find_driver_arg(const char *name)
{
    unsigned int holder =
        sctrlHENFindFunction("sceIOFileManager", "IoFileMgrForKernel", NID_IOFILEMGR_LOOKUP_HOLDER);
    unsigned int lookup = holder ? first_jal_target(holder) : 0;
    unsigned int *(*find)(const char *) = (unsigned int *(*)(const char *))lookup;
    unsigned int *node;

    if (!lookup)
        return NULL;
    node = find(name);
    return node ? (PspIoDrvArg *)(node + 1) : NULL;
}

static int drv_call(PspIoDrvArg *arg, int init)
{
    if (arg == NULL || arg->drv == NULL || arg->drv->funcs == NULL)
        return -1;
    if (init)
        return arg->drv->funcs->IoInit ? arg->drv->funcs->IoInit(arg) : -1;
    return arg->drv->funcs->IoExit ? arg->drv->funcs->IoExit(arg) : -1;
}

int pw_flash_available(void)
{
    unsigned int k1 = set_k1(0);
    int ok = find_driver_arg("lflash") != NULL && find_driver_arg("flashfat") != NULL;

    set_k1(k1);
    return ok;
}

int pw_flash_reinit(pw_flash_report *r, int remount)
{
    PspIoDrvArg *lflash, *flashfat;
    unsigned int k1;

    r->remount = remount;
    r->unassign0 = r->unassign1 = r->assign0 = r->assign1 = 0;
    if (remount) {
        r->unassign0 = sceIoUnassign("flash0:");
        r->unassign1 = sceIoUnassign("flash1:");
    }

    k1 = set_k1(0);
    lflash = find_driver_arg("lflash");
    flashfat = find_driver_arg("flashfat");
    r->exit_lflash = drv_call(lflash, 0);
    r->exit_flashfat = drv_call(flashfat, 0);
    r->init_lflash = drv_call(lflash, 1);
    r->init_flashfat = drv_call(flashfat, 1);
    set_k1(k1);

    if (remount) {
        r->assign0 = sceIoAssign("flash0:", "lflash0:0,0", "flashfat0:", IOASSIGN_RDONLY, NULL, 0);
        r->assign1 = sceIoAssign("flash1:", "lflash0:0,1", "flashfat1:", IOASSIGN_RDWR, NULL, 0);
    }
    r->found_lflash = lflash != NULL;
    r->found_flashfat = flashfat != NULL;
    if (!lflash || !flashfat || r->init_lflash < 0 || r->init_flashfat < 0)
        return -1;
    return (r->assign0 >= 0 && r->assign1 >= 0) ? 0 : -1;
}

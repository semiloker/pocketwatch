// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_BOOTPICK_H
#define PW_BOOTPICK_H

/*
 * Where the PSP starts (ADR-026, docs/research/m7-boot-shell.md): the XMB plugin decides as ARK-4
 * loads it, before vsh_module starts. To start Pocketwatch Home it writes Home's file name into
 * ARK-4's config (ARKConfig.launcher); ARK-4 then starts that file instead of the XMB
 * (core/systemctrl/src/loadercore.c, InitKernelStartModule). Portable: the decision and the
 * config's bytes, without the PSP.
 */

/* Home in ARK-4's folder: ARK-4's launcher name is relative to that folder. */
#define PW_BOOT_HOME_FILE "PWHOME.PBP"

/* ARK-4's ARKConfig (common/include/ark.h) as bytes: what is read and written of it. */
#define PW_ARK_CONFIG_BUF  256 /* sctrlArkGetConfig copies sizeof(ARKConfig) = 148 */
#define PW_ARK_MAGIC       0xB00B1E55u
#define PW_ARK_PATH_OFF    4 /* char arkpath[108] */
#define PW_ARK_PATH_LEN    108
#define PW_ARK_EXPLOIT_OFF 112 /* char exploit_id[12] */
#define PW_ARK_LAUNCH_OFF  124 /* char launcher[20] */
#define PW_ARK_LAUNCH_LEN  20
#define PW_ARK_MODE_OFF    144 /* unsigned char exec_mode: (mode & 0x0C) == 0x04 on a PSP */
#define PW_ARK_RECOV_OFF   145 /* unsigned char recovery */

typedef struct {
    int want_home;    /* config.txt: start = home */
    int xmb_once;     /* Home asked for the XMB this one time (its Exit, HOME -> Quit) */
    int held;         /* L held as the plugin starts: the XMB this time */
    int pending;      /* the last start of Home never reported back: it may be broken */
    int home_present; /* PWHOME.PBP is in ARK-4's folder */
    int ark_ok;       /* pw_boot_ark_check passed */
} pw_boot_in;

/* 1: start Home, 0: the XMB. *why (if given) names the reason, for the log. */
int pw_boot_pick(const pw_boot_in *in, const char **why);

/* 0 when cfg is an ARKConfig of a PSP as this code expects it (magic, a ms0:/ or ef0:/ folder,
   no launcher or recovery set already); < 0 otherwise - then nothing may be written into it. */
int pw_boot_ark_check(const unsigned char *cfg);

/* Write name (< 20 bytes) as the launcher. Returns < 0 if it does not fit. */
int pw_boot_ark_set(unsigned char *cfg, const char *name);

/* ARK-4's folder in cfg ("ms0:/PSP/SAVEDATA/ARK_01234/"); valid after pw_boot_ark_check. */
const char *pw_boot_ark_folder(const unsigned char *cfg);

#endif

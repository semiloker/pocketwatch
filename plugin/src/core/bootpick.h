// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_BOOTPICK_H
#define PW_BOOTPICK_H

/*
 * Where the PSP starts (ADR-026, ADR-027, docs/research/m7-boot-shell.md): the XMB plugin decides
 * as ARK-4 loads it, and writes Home's file name into ARK-4's config (ARKConfig.launcher) once
 * ARK-4's own processSettings has run; ARK-4 then starts that file instead of the XMB
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
    int xmb_once;     /* Home asked for the XMB just now (its Exit, HOME -> Quit) */
    int held;         /* L held as the plugin starts: the XMB this time */
    int pending;      /* the last start of Home never reported back: it may be broken */
    int home_present; /* PWHOME.PBP is in ARK-4's folder */
    int ark_ok;       /* pw_boot_ark_layout passed */
} pw_boot_in;

/* 1: start Home, 0: the XMB. *why (if given) names the reason, for the log. */
int pw_boot_pick(const pw_boot_in *in, const char **why);

/* 0 when cfg is an ARKConfig of a PSP as this code expects it (magic, terminated strings, a ms0:/
   or ef0:/ folder, a PSP); < 0 otherwise. */
int pw_boot_ark_layout(const unsigned char *cfg);

/* pw_boot_ark_layout, and no launcher or recovery set (ARK-4's own launcher mode, recovery and
   ProShell win): 0 only then may the launcher be written. */
int pw_boot_ark_check(const unsigned char *cfg);

/* The xmb-once marker's text (the RTC tick, in microseconds, Home wrote it at) is fresh: written
   at most PW_BOOT_MARKER_US before now - so a marker never outlives the start it was meant for. */
#define PW_BOOT_MARKER_US (120ull * 1000 * 1000)
int pw_boot_marker_fresh(const char *text, unsigned long long now);

/* Write name (< 20 bytes) as the launcher. Returns < 0 if it does not fit. */
int pw_boot_ark_set(unsigned char *cfg, const char *name);

/* ARK-4's folder in cfg ("ms0:/PSP/SAVEDATA/ARK_01234/"); valid after pw_boot_ark_check. */
const char *pw_boot_ark_folder(const unsigned char *cfg);

#endif

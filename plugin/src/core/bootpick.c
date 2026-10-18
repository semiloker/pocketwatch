// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "core/bootpick.h"

int pw_boot_pick(const pw_boot_in *in, const char **why)
{
    const char *r = "Pocketwatch Home";
    int home = 0;

    if (!in->want_home)
        r = "start = xmb";
    else if (in->xmb_once)
        r = "the XMB asked from Home";
    else if (in->held)
        r = "L held";
    else if (in->pending)
        r = "Home did not start last time";
    else if (!in->home_present)
        r = PW_BOOT_HOME_FILE " is not in ARK-4's folder";
    else if (!in->ark_ok)
        r = "ARK-4's config is not as expected";
    else
        home = 1;
    if (why)
        *why = r;
    return home;
}

/* A NUL inside the n bytes at s. */
static int terminated(const unsigned char *s, int n)
{
    return memchr(s, 0, (unsigned int)n) != 0;
}

int pw_boot_ark_check(const unsigned char *cfg)
{
    unsigned int magic = (unsigned int)cfg[0] | (unsigned int)cfg[1] << 8 |
                         (unsigned int)cfg[2] << 16 | (unsigned int)cfg[3] << 24;
    const char *path = (const char *)cfg + PW_ARK_PATH_OFF;

    if (magic != PW_ARK_MAGIC)
        return -1;
    if (!terminated(cfg + PW_ARK_PATH_OFF, PW_ARK_PATH_LEN) ||
        !terminated(cfg + PW_ARK_EXPLOIT_OFF, PW_ARK_LAUNCH_OFF - PW_ARK_EXPLOIT_OFF) ||
        !terminated(cfg + PW_ARK_LAUNCH_OFF, PW_ARK_LAUNCH_LEN))
        return -2;
    if ((strncmp(path, "ms0:/", 5) != 0 && strncmp(path, "ef0:/", 5) != 0) ||
        path[strlen(path) - 1] != '/')
        return -3;
    if ((cfg[PW_ARK_MODE_OFF] & 0x0C) != 0x04) /* not a PSP (a PS Vita: no XMB of its own) */
        return -4;
    if (cfg[PW_ARK_LAUNCH_OFF] != 0 || cfg[PW_ARK_RECOV_OFF] != 0)
        return -5; /* ARK-4's own launcher mode or recovery: theirs to decide */
    return 0;
}

int pw_boot_ark_set(unsigned char *cfg, const char *name)
{
    unsigned int n = (unsigned int)strlen(name);

    if (n >= PW_ARK_LAUNCH_LEN)
        return -1;
    memset(cfg + PW_ARK_LAUNCH_OFF, 0, PW_ARK_LAUNCH_LEN);
    memcpy(cfg + PW_ARK_LAUNCH_OFF, name, n);
    return 0;
}

const char *pw_boot_ark_folder(const unsigned char *cfg)
{
    return (const char *)cfg + PW_ARK_PATH_OFF;
}

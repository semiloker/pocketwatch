// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_VSH_XMBTYPES_H
#define PW_VSH_XMBTYPES_H

/*
 * The XMB's own structures as ARK-4's xmbctrl uses them on 6.61 (extras/modules/xmbctrl/include/
 * main.h; XmbControl by Total_Noob, Frostegater, codestation, GPL-3.0-or-later). Layouts only.
 */

typedef unsigned short pw_wchar; /* the XMB's text: UCS-2 */

typedef struct {
    char text[48];
    int play_sound;
    int action;
    int action_arg;
} pw_xmb_context_item;

/* An item of an XMB column (vsh_module's AddVshItem). */
typedef struct {
    int id;
    int relocate;
    int action;
    int action_arg;
    pw_xmb_context_item *context;
    char *subtitle;
    int unk;
    char play_sound;
    char memstick;
    char umd_icon;
    char image[4];
    char image_shadow[4];
    char image_glow[4];
    char text[0x25]; /* a text key for scePafGetText */
} pw_xmb_item;

/* A row of a Settings page (sysconf_plugin). */
typedef struct {
    void *unk;
    int id;
    char *regkey; /* the name the page asks vshGet/SetRegistryValue for */
    char *text;   /* a text key for scePafGetText */
    char *subtitle;
    char *page; /* the RCO page that lists the row's values */
} pw_sysconf_item;

/* An entry of a loaded RCO resource tree. */
typedef struct {
    unsigned char id;
    unsigned char type;
    unsigned short unk1;
    unsigned int label;
    unsigned int param;
    unsigned int first_child;
    int child_count;
    unsigned int next_entry;
    unsigned int prev_entry;
    unsigned int parent;
    unsigned int unknown[2];
} pw_rco_entry;

/* The firmware's functions we take the address of (src/stubs.S). */
pw_wchar *scePafGetText(void *a0, char *name);
int PAF_Resource_GetPageNodeByID(void *resource, char *name, pw_rco_entry **child);
int PAF_Resource_ResolveRefWString(void *resource, unsigned int *data, int *a2, char **string,
                                   int *t0);

#endif

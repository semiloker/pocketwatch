// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Pocketwatch in the XMB (VSH-REQ-006..008, ADR-021, ADR-024): Settings -> "Pocketwatch" (the
 * plugin's settings) and "Battery", Game -> "Play Time", each as the XMB's own option list.
 *
 * The mechanism is ARK-4's xmbctrl's, which shows "Custom Firmware Settings" the same way
 * (extras/modules/xmbctrl/main.c; XmbControl by Total_Noob, Frostegater, codestation and the
 * ARK-4 team, GPL-3.0-or-later): an item added to a column of vsh_module, whose action opens the
 * System Settings page of sysconf_plugin, filled with other rows whose values come from hooked
 * registry calls; the instruction patterns that find those call sites are xmbctrl's.
 *
 * What is ours is how it lives next to xmbctrl, which patches the same call sites and is always
 * loaded before any plugin: every site is located before xmbctrl's start-module handler runs
 * (pw_xmb_*_before), xmbctrl patches, then each site is wrapped around whatever it calls now
 * (pw_xmb_*_after): xmbctrl's function, or the firmware's when xmbctrl is absent. A module in
 * which any site is missing, or whose sites disagree, is left untouched (and the log says why).
 */
#include <string.h>

#include "xlog.h"
#include "xmb.h"
#include "xmbtypes.h"
#include "ui/i18n.h"
#include "xsettings.h"

#define CONSOLE_ITEM   "msgtop_sysconf_console" /* System Settings: Pocketwatch, Battery after it */
#define SAVEDATA_ITEM  "msgtop_game_savedata"   /* Saved Data Utility: Play Time after it */
#define VALUES_PAGE    "page_psp_config_umd_autoboot" /* the RCO page that lists a row's values */
#define CONSOLE_ACTION 2 /* System Settings' action when it was not seen yet (xmbctrl main.h) */

/* Our items: text keys, ids (xmbctrl uses 81-84), action arguments (xmbctrl 0x1000-0x1005); each
   opens System Settings' page with the rows of one of our pages. */
enum { ITEM_SETTINGS, ITEM_BATTERY, ITEM_PLAYTIME, ITEMS };
static const struct {
    const char *key;
    int id, arg, page;
} g_ours[ITEMS] = {
    {"pw_xmb_settings", 90, 0x1500, PWX_PAGE_SETTINGS},
    {"pw_xmb_battery", 91, 0x1501, PWX_PAGE_BATTERY},
    {"pw_xmb_playtime", 92, 0x1502, PWX_PAGE_PLAYTIME},
};
#define MAGIC_TEXT 0xDEAD /* an RCO text that is a pointer to our UTF-8 (as xmbctrl's) */

#define LW(a)        (*(volatile unsigned int *)(a))
#define SW(v, a)     (*(volatile unsigned int *)(a) = (unsigned int)(v))
#define JAL(f)       (0x0C000000u | (((unsigned int)(f) >> 2) & 0x03FFFFFFu))
#define J(f)         (0x08000000u | (((unsigned int)(f) >> 2) & 0x03FFFFFFu))
#define OP(i)        ((i) & 0xFC000000u)
#define TARGET(i)    (((i) & 0x03FFFFFFu) << 2) /* user memory: the top bits are 0 */
#define USER_ADDR(a) ((a) >= 0x08800000u && (a) < 0x0A000000u)

void sctrlFlushCache(void);
void *sce_paf_private_malloc(int size);

/* ---- call sites ---- */

#define MAX_SITES 4

typedef struct {
    unsigned int at[MAX_SITES];
    int n, extra; /* extra: more matches than room */
} sites;

static void add_site(sites *s, unsigned int a)
{
    if (s->n < MAX_SITES)
        s->at[s->n++] = a;
    else
        s->extra = 1;
}

/* The function every site calls (op: a jal, or the j of an import stub), or 0 when a site is
   something else or they disagree. */
static unsigned int common_target(const sites *s, unsigned int op)
{
    unsigned int t = 0;

    if (s->n == 0 || s->extra)
        return 0;
    for (int i = 0; i < s->n; i++) {
        unsigned int ins = LW(s->at[i]), to = TARGET(ins);

        if (OP(ins) != op || !USER_ADDR(to) || (t && to != t))
            return 0;
        t = to;
    }
    return t;
}

/* 1 the first time bit is asked for. */
static int first(unsigned int *seen, unsigned int bit)
{
    if (*seen & bit)
        return 0;
    *seen |= bit;
    return 1;
}

static void point_to(const sites *s, unsigned int op, const void *f)
{
    for (int i = 0; i < s->n; i++) {
        SW(op == 0x0C000000u ? JAL(f) : J(f), s->at[i]);
        if (op != 0x0C000000u)
            SW(0, s->at[i] + 4); /* an import stub: j f; nop */
    }
}

#define OP_JAL 0x0C000000u
#define OP_J   0x08000000u

/* ---- text ---- */

#define TEXT_BUFS 8
#define TEXT_MAX  64

/* UTF-8 as the XMB's UCS-2, in one of a few rotating buffers (the XMB copies what it gets). */
static pw_wchar *ucs2(const char *s)
{
    static pw_wchar buf[TEXT_BUFS][TEXT_MAX];
    static int next;
    pw_wchar *out = buf[next];
    const unsigned char *u = (const unsigned char *)s;
    int n = 0;

    next = (next + 1) % TEXT_BUFS;
    while (u && *u && n < TEXT_MAX - 1) {
        unsigned int c = *u++;

        if (c >= 0xE0 && u[0] && u[1]) {
            c = ((c & 0x0F) << 12) | ((u[0] & 0x3F) << 6) | (u[1] & 0x3F);
            u += 2;
        } else if (c >= 0xC0 && u[0]) {
            c = ((c & 0x1F) << 6) | (u[0] & 0x3F);
            u += 1;
        }
        out[n++] = (pw_wchar)c;
    }
    out[n] = 0;
    return out;
}

/* ---- vsh_module: the column item and its action ---- */

static struct {
    sites add, exec, unload, text;
    int found;
} g_vsh;

static unsigned int g_next_add, g_next_exec, g_next_unload, g_next_text_vsh;
static int g_added;                         /* 1 << ITEM_*: in this vsh_module's columns */
static int g_console_action, g_console_arg; /* what System Settings does */
static int g_open;                          /* sysconf_plugin is showing our page */
static int g_unload;                        /* sysconf_plugin must load anew (other rows) */
static int g_texts;                         /* the translations were read */
static pw_xmb_item g_item[ITEMS];

/* Add ours[which] after `like` (whose icon it takes) in the same column. */
static void add_ours(int (*next)(void *, int, pw_xmb_item *), void *a0, int topitem,
                     const pw_xmb_item *like, int which)
{
    pw_xmb_item *it = &g_item[which];

    memcpy(it, like, sizeof(*it));
    it->id = g_ours[which].id;
    it->action = g_console_action ? g_console_action : CONSOLE_ACTION;
    it->action_arg = g_ours[which].arg;
    it->play_sound = 1;
    it->context = 0;
    strcpy(it->text, g_ours[which].key);
    next(a0, topitem, it);
    g_added |= 1 << which;
}

static int add_item(void *a0, int topitem, pw_xmb_item *item)
{
    int (*next)(void *, int, pw_xmb_item *) = (void *)g_next_add;
    int ret = next(a0, topitem, item);

    if (!g_texts) { /* the column names are asked for as soon as the items exist */
        g_texts = 1;
        pwx_texts_load();
    }
    if (!(g_added & (1 << ITEM_SETTINGS)) && strcmp(item->text, CONSOLE_ITEM) == 0) {
        g_console_action = item->action;
        g_console_arg = item->action_arg;
        add_ours(next, a0, topitem, item, ITEM_SETTINGS);
        add_ours(next, a0, topitem, item, ITEM_BATTERY);
        pwx_log("xmb: Pocketwatch and Battery added after System Settings (action %d, arg %d)",
                g_console_action, g_console_arg);
        pwx_log_flush();
    } else if (!(g_added & (1 << ITEM_PLAYTIME)) && strcmp(item->text, SAVEDATA_ITEM) == 0) {
        add_ours(next, a0, topitem, item, ITEM_PLAYTIME);
        pwx_log("xmb: Play Time added after Saved Data Utility");
        pwx_log_flush();
    }
    return ret;
}

static int execute_action(int action, int arg)
{
    int (*next)(int, int) = (void *)g_next_exec;
    int console = g_console_action ? g_console_action : CONSOLE_ACTION;

    if (g_added && action == console) { /* a Settings page */
        int page = PWX_PAGE_NONE;

        for (int i = 0; i < ITEMS; i++)
            if (arg == g_ours[i].arg)
                page = g_ours[i].page;
        if (page != PWX_PAGE_NONE)
            arg = g_console_arg ? g_console_arg : CONSOLE_ACTION; /* System Settings' page */
        if (page != pwx_page())
            g_unload = 1; /* its rows are cached: load it anew */
        g_open = page != PWX_PAGE_NONE;
        pwx_page_open(page);
        if (g_open)
            pwx_log_flush();
    }
    return next(action, arg);
}

static int unload_module(int skip)
{
    int (*next)(int) = (void *)g_next_unload;

    if (g_unload) {
        skip = -1;
        g_unload = 0;
    }
    return next(skip);
}

static pw_wchar *get_text_vsh(void *a0, char *name)
{
    pw_wchar *(*next)(void *, char *) = (void *)g_next_text_vsh;

    if (name && strcmp(name, g_ours[ITEM_SETTINGS].key) == 0)
        return ucs2("Pocketwatch");
    if (name && strcmp(name, g_ours[ITEM_BATTERY].key) == 0)
        return ucs2(pw_tr(PW_S_BATTERY));
    if (name && strcmp(name, g_ours[ITEM_PLAYTIME].key) == 0)
        return ucs2(pw_tr(PW_S_PLAYTIME));
    return next(a0, name);
}

void pw_xmb_vsh_before(unsigned int text, unsigned int size)
{
    unsigned int gettext = LW(&scePafGetText); /* our resolved stub: j <the function> */

    memset(&g_vsh, 0, sizeof(g_vsh));
    unsigned int seen = 0;

    /* The first match of each pattern: xmbctrl patches one site per pattern (its counter equals
       the number of patterns) and stops. */
    for (unsigned int a = text + 4; a + 4 <= text + size; a += 4) {
        unsigned int d = LW(a);

        if (d == 0x00063100 && first(&seen, 1))
            add_site(&g_vsh.add, a + 12);
        else if (d == 0x3A14000F && a >= text + 108 && first(&seen, 2))
            add_site(&g_vsh.exec, a - 108);
        else if (d == 0x8E050038 && first(&seen, 4))
            add_site(&g_vsh.exec, a + 4);
        else if (d == 0xAC520124 && first(&seen, 8))
            add_site(&g_vsh.unload, a + 4);
        else if (d == gettext && first(&seen, 16))
            add_site(&g_vsh.text, a);
    }
    g_vsh.found = 1;
}

void pw_xmb_vsh_after(void)
{
    unsigned int add, exec, unload, text;

    if (!g_vsh.found)
        return;
    g_vsh.found = 0;
    add = common_target(&g_vsh.add, OP_JAL);
    exec = common_target(&g_vsh.exec, OP_JAL);
    unload = common_target(&g_vsh.unload, OP_JAL);
    text = common_target(&g_vsh.text, OP_J);
    if (!add || !exec || !unload || !text) {
        pwx_log("xmb: vsh_module not patched: add %d/%08X exec %d/%08X unload %d/%08X text "
                "%d/%08X",
                g_vsh.add.n, add, g_vsh.exec.n, exec, g_vsh.unload.n, unload, g_vsh.text.n, text);
        return;
    }
    g_next_add = add;
    g_next_exec = exec;
    g_next_unload = unload;
    g_next_text_vsh = text;
    g_added = g_open = g_unload = g_texts = 0;
    g_console_action = g_console_arg = 0;
    point_to(&g_vsh.add, OP_JAL, add_item);
    point_to(&g_vsh.exec, OP_JAL, execute_action);
    point_to(&g_vsh.unload, OP_JAL, unload_module);
    point_to(&g_vsh.text, OP_J, get_text_vsh);
    sctrlFlushCache();
    pwx_log("xmb: vsh_module patched (add %08X exec %08X unload %08X text %08X)", add, exec, unload,
            text);
}

/* ---- sysconf_plugin: our rows on System Settings' page ---- */

static struct {
    sites get_item, reg_get, reg_set, page, resolve, text;
    unsigned int add_fn, init_fn, init_slot, opt_unk, opt;
    int found;
} g_sc;

static unsigned int g_next_get_item, g_next_reg_get, g_next_reg_set, g_next_page, g_next_resolve,
    g_next_text_sc, g_next_init, g_add_fn, g_opt_unk, g_opt;
static int g_mode = -1;          /* the row whose values the page lists */
static unsigned int g_backup[4]; /* the values page as the firmware made it */
static pw_rco_entry *g_values;   /* our values' entries, made once per sysconf_plugin */
static unsigned int g_values_cap;

#define MAX_VALUES 16

static void add_row(int row)
{
    pw_sysconf_item *it = sce_paf_private_malloc(sizeof(pw_sysconf_item));
    void (*add)(unsigned int *, pw_sysconf_item **) = (void *)g_add_fn;

    if (!it)
        return;
    it->id = 5;
    it->unk = (void *)g_opt_unk;
    it->regkey = (char *)pwx_row_key(row);
    it->text = (char *)pwx_row_key(row);
    it->subtitle = 0;
    it->page = VALUES_PAGE;
    ((unsigned int *)g_opt)[2] = 1;
    add((unsigned int *)g_opt, &it);
}

/* The page's init: our rows instead of System Settings' (its arguments pass through untouched). */
static void on_init_menu(int a0, int a1, int a2, int a3)
{
    if (g_open) {
        if (((unsigned int *)g_opt)[2] == 0)
            for (int row = 0; row < pwx_rows(); row++)
                add_row(row);
        pwx_log_flush();
        return;
    }
    ((void (*)(int, int, int, int))g_next_init)(a0, a1, a2, a3);
}

static pw_sysconf_item *get_item(void *a0, void *a1)
{
    pw_sysconf_item *(*next)(void *, void *) = (void *)g_next_get_item;
    pw_sysconf_item *item = next(a0, a1);

    if (g_open && item && item->text) {
        int row = pwx_row_by_key(item->text);

        if (row >= 0)
            g_mode = row;
    }
    return item;
}

static int reg_get(unsigned int *option, char *name, void *arg2, int size, int *value)
{
    int (*next)(unsigned int *, char *, void *, int, int *) = (void *)g_next_reg_get;
    int row = g_open ? pwx_row_by_key(name) : -1;

    if (row >= 0) {
        g_mode = row;
        *value = pwx_row_get(row);
        return 0;
    }
    return next(option, name, arg2, size, value);
}

static int reg_set(unsigned int *option, char *name, int size, int *value)
{
    int (*next)(unsigned int *, char *, int, int *) = (void *)g_next_reg_set;
    int row = g_open ? pwx_row_by_key(name) : -1;

    if (row >= 0) {
        pwx_row_set(row, *value);
        pwx_log_flush();
        return 0;
    }
    return next(option, name, size, value);
}

/* The values page lists our row's values instead of its own (xmbctrl's HijackContext). */
static void values_of(pw_rco_entry *src, int row)
{
    pw_rco_entry *plane = (pw_rco_entry *)((unsigned int)src + src->first_child);
    pw_rco_entry *mlist = (pw_rco_entry *)((unsigned int)plane + plane->first_child);
    unsigned int *mlist_param = (unsigned int *)((unsigned int)mlist + mlist->param);

    if (!g_backup[0] && !g_backup[1] && !g_backup[2] && !g_backup[3]) {
        g_backup[0] = mlist->first_child;
        g_backup[1] = (unsigned int)mlist->child_count;
        g_backup[2] = mlist_param[16];
        g_backup[3] = mlist_param[18];
    }
    if (row >= 0) {
        pw_rco_entry *base = (pw_rco_entry *)((unsigned int)mlist + mlist->first_child);
        int n = pwx_row_count(row);
        pw_rco_entry *item;
        unsigned int *item_param;

        if (n > MAX_VALUES)
            n = MAX_VALUES;
        if (!g_values || g_values_cap < base->next_entry * (unsigned int)n) {
            /* made once for the page and written again for each row (xmbctrl allocates every
               time the list opens) */
            g_values_cap = base->next_entry * MAX_VALUES;
            g_values = sce_paf_private_malloc((int)g_values_cap);
        }
        if (!g_values)
            return;
        item = g_values;
        item_param = (unsigned int *)((unsigned int)item + base->param);
        mlist->first_child = (unsigned int)item - (unsigned int)mlist;
        mlist->child_count = n;
        mlist_param[16] = 13;
        mlist_param[18] = 6;
        for (int i = 0; i < n; i++) {
            memcpy(item, base, base->next_entry);
            item_param[0] = MAGIC_TEXT;
            item_param[1] = (unsigned int)pwx_row_option(row, i);
            if (i != 0)
                item->prev_entry = item->next_entry;
            if (i == n - 1)
                item->next_entry = 0;
            item = (pw_rco_entry *)((unsigned int)item + base->next_entry);
            item_param = (unsigned int *)((unsigned int)item + base->param);
        }
    } else {
        mlist->first_child = g_backup[0];
        mlist->child_count = (int)g_backup[1];
        mlist_param[16] = g_backup[2];
        mlist_param[18] = g_backup[3];
    }
    sctrlFlushCache();
}

static int page_node(void *resource, char *name, pw_rco_entry **child)
{
    int (*next)(void *, char *, pw_rco_entry **) = (void *)g_next_page;
    int ret = next(resource, name, child);

    if (g_open && name && child && *child && strcmp(name, VALUES_PAGE) == 0)
        values_of(*child, g_mode);
    return ret;
}

static int resolve_text(void *resource, unsigned int *data, int *a2, char **string, int *t0)
{
    int (*next)(void *, unsigned int *, int *, char **, int *) = (void *)g_next_resolve;

    if (data && data[0] == MAGIC_TEXT) {
        *(pw_wchar **)string = ucs2((const char *)data[1]);
        return 0;
    }
    return next(resource, data, a2, string, t0);
}

static pw_wchar *get_text_sc(void *a0, char *name)
{
    pw_wchar *(*next)(void *, char *) = (void *)g_next_text_sc;

    if (g_open && name) {
        int row = pwx_row_by_key(name);

        if (row >= 0)
            return ucs2(pwx_row_label(row));
        if (strcmp(name, CONSOLE_ITEM) == 0) /* the page's title */
            return ucs2(pwx_page_title());
    }
    return next(a0, name);
}

void pw_xmb_sysconf_before(unsigned int text, unsigned int size)
{
    unsigned int page = LW(&PAF_Resource_GetPageNodeByID),
                 resolve = LW(&PAF_Resource_ResolveRefWString), gettext = LW(&scePafGetText);

    unsigned int seen = 0;

    memset(&g_sc, 0, sizeof(g_sc));
    for (unsigned int a = text + 16; a + 8 <= text + size; a += 4) {
        unsigned int d = LW(a);

        if (d == 0x24420008 && LW(a - 4) == 0x00402821 && first(&seen, 1))
            g_sc.add_fn = a - 36;
        else if (d == 0xAFBF0060 && LW(a + 4) == 0xAFB3005C && LW(a - 12) == 0xAFB00050 &&
                 first(&seen, 2))
            g_sc.init_fn = a - 20;
        else if (d == 0x01202821 && first(&seen, 4)) {
            add_site(&g_sc.reg_get, a + 8);
            add_site(&g_sc.reg_set, a + 44);
        } else if (d == 0x2C620012 && LW(a - 4) == 0x00408821 && first(&seen, 8))
            add_site(&g_sc.get_item, a - 16);
        else if (g_sc.init_fn && d == g_sc.init_fn && first(&seen, 16))
            g_sc.init_slot = a;
        else if (d == page && first(&seen, 32))
            add_site(&g_sc.page, a);
        else if (d == resolve && first(&seen, 64))
            add_site(&g_sc.resolve, a);
        else if (d == gettext && first(&seen, 128))
            add_site(&g_sc.text, a);
    }
    for (unsigned int a = text + 0x33000; a < text + 0x40000; a++) /* as xmbctrl */
        if (memcmp((const void *)a, "fiji", 5) == 0) {
            g_sc.opt_unk = a + 216;
            if (LW(g_sc.opt_unk + 4) == 0)
                g_sc.opt_unk -= 4; /* TT/DT firmware */
            g_sc.opt = g_sc.opt_unk + 0x4CC;
            break;
        }
    g_sc.found = 1;
}

void pw_xmb_sysconf_after(void)
{
    unsigned int get_item_to, reg_get_to, reg_set_to, page_to, resolve_to, text_to, init_to = 0;

    if (!g_sc.found)
        return;
    g_sc.found = 0;
    get_item_to = common_target(&g_sc.get_item, OP_JAL);
    reg_get_to = common_target(&g_sc.reg_get, OP_JAL);
    reg_set_to = common_target(&g_sc.reg_set, OP_JAL);
    page_to = common_target(&g_sc.page, OP_J);
    resolve_to = common_target(&g_sc.resolve, OP_J);
    text_to = common_target(&g_sc.text, OP_J);
    if (g_sc.init_slot && USER_ADDR(LW(g_sc.init_slot)))
        init_to = LW(g_sc.init_slot);
    if (!get_item_to || !reg_get_to || !reg_set_to || !page_to || !resolve_to || !text_to ||
        !init_to || !g_sc.add_fn || !g_sc.opt) {
        pwx_log("xmb: sysconf_plugin not patched: item %08X get %08X set %08X page %08X resolve "
                "%08X text %08X init %08X add %08X opt %08X",
                get_item_to, reg_get_to, reg_set_to, page_to, resolve_to, text_to, init_to,
                g_sc.add_fn, g_sc.opt);
        return;
    }
    g_next_get_item = get_item_to;
    g_next_reg_get = reg_get_to;
    g_next_reg_set = reg_set_to;
    g_next_page = page_to;
    g_next_resolve = resolve_to;
    g_next_text_sc = text_to;
    g_next_init = init_to;
    g_add_fn = g_sc.add_fn;
    g_opt_unk = g_sc.opt_unk;
    g_opt = g_sc.opt;
    g_mode = -1;
    memset(g_backup, 0, sizeof(g_backup)); /* a new instance: its own resources */
    g_values = 0;
    g_values_cap = 0;
    point_to(&g_sc.get_item, OP_JAL, get_item);
    point_to(&g_sc.reg_get, OP_JAL, reg_get);
    point_to(&g_sc.reg_set, OP_JAL, reg_set);
    point_to(&g_sc.page, OP_J, page_node);
    point_to(&g_sc.resolve, OP_J, resolve_text);
    point_to(&g_sc.text, OP_J, get_text_sc);
    SW(on_init_menu, g_sc.init_slot);
    sctrlFlushCache();
    pwx_log("xmb: sysconf_plugin patched%s", g_open ? " (our page)" : "");
}

// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * xmbprobe - a research tool for M7's XMB side (docs/research/m7-xmb-context-menu.md), not part
 * of Pocketwatch and never shipped. A VSH plugin (user mode, like pocketwatch_xmb.prx) that
 *
 *  - logs every call of vsh_module's OnXmbContextMenu and OnXmbPush: their two arguments, the
 *    words they point to and the caller. It answers whether a game's Triangle menu goes through
 *    vsh_module at all - if it does, "Continue from Save State" can be added there, in a module
 *    Pocketwatch already patches;
 *  - dumps vsh_module and game_plugin_module as they are in memory (each segment, the module
 *    record, the import and export tables) for disassembly.
 *
 * Output: ms0:/seplugins/pocketwatch/probe/ (probe.log, <module>.seg<n>.bin, <module>.txt). The
 * dumps are Sony's code: they stay on the owner's card and computer, never in the repository
 * (docs/AI_WORKFLOW.md section 7).
 *
 * The two functions are found as ARK-4's xmbctrl finds them on 6.61 (extras/modules/xmbctrl/
 * main.c, GPL-3.0-or-later): OnXmbContextMenu 24 bytes before the word 0x00021202, OnXmbPush 124
 * bytes before 0x9042001C; their entries in vsh_module's function table are the first words
 * equal to them above text + 0x50000. Each entry is located before the older start-module
 * handlers run (xmbctrl, pocketwatch_xmb) and wrapped after them, whatever they put there.
 */
#include <pspiofilemgr.h>
#include <stdarg.h>
#include <pspkernel.h>
#include <psploadcore.h>
#include <string.h>

#include "stb_sprintf.h"

PSP_MODULE_INFO("PocketwatchProbe", 0x0007, 1, 0);

#define DIR      "ms0:/seplugins/pocketwatch/probe"
#define LOG_CAP  32768
#define WAIT_S   600 /* how long the dumps wait for game_plugin_module */
#define LW(a)    (*(volatile unsigned int *)(a))
#define USER(a)  ((unsigned int)(a) >= 0x08800000u && (unsigned int)(a) < 0x0A000000u)
#define DUMP_MAX 4 /* modules */

typedef int (*start_handler)(SceModule *mod);
typedef int (*xmb_fn)(void *a0, void *a1);

start_handler sctrlHENSetStartModuleHandler(start_handler handler); /* stubs.S */
void sctrlFlushCache(void);

static start_handler g_previous;

/* ---- the log: lines in RAM, written by the probe's thread ---- */

static char g_log[LOG_CAP];
static volatile int g_len;

static void note(const char *fmt, ...)
{
    va_list ap;
    int n;

    if (g_len >= LOG_CAP - 256)
        return; /* full: the first calls are the ones that matter */
    va_start(ap, fmt);
    n = stbsp_vsnprintf(g_log + g_len, LOG_CAP - g_len - 1, fmt, ap);
    va_end(ap);
    if (n > 0)
        g_len += n;
}

/* 16 words at p, with the bytes as text where they are printable. */
static void words(const char *what, const void *p)
{
    const unsigned char *b = p;

    if (!USER(p) || !USER((unsigned int)p + 63)) {
        note("  %s %08X (not user memory)\n", what, (unsigned int)p);
        return;
    }
    for (int r = 0; r < 4; r++) {
        char text[17];

        for (int i = 0; i < 16; i++)
            text[i] = b[r * 16 + i] >= 32 && b[r * 16 + i] < 127 ? (char)b[r * 16 + i] : '.';
        text[16] = 0;
        note("  %s+%02X %08X %08X %08X %08X %s\n", what, r * 16, LW(b + r * 16), LW(b + r * 16 + 4),
             LW(b + r * 16 + 8), LW(b + r * 16 + 12), text);
    }
}

/* ---- the two XMB functions ---- */

typedef struct {
    const char *name;
    unsigned int pattern;
    int back;           /* the function: this many bytes before the pattern */
    unsigned int fn;    /* the firmware's function */
    unsigned int entry; /* its word in vsh_module's function table */
    xmb_fn next;        /* what the entry held after the older handlers */
    int calls;
} hooked;

static hooked g_ctx = {"OnXmbContextMenu", 0x00021202, 24, 0, 0, 0, 0};
static hooked g_push = {"OnXmbPush", 0x9042001C, 124, 0, 0, 0, 0};

static int called(hooked *h, void *a0, void *a1, unsigned int ra)
{
    int n = ++h->calls;

    note("%s #%d from %08X: a0=%08X a1=%08X\n", h->name, n, ra, (unsigned int)a0, (unsigned int)a1);
    if (n <= 40) { /* the words of the first calls (a few presses of Triangle) */
        words("a0", a0);
        words("a1", a1);
        if (USER(a1) && USER(LW(a1)))
            words("*a1", (void *)LW(a1));
    }
    return h->next(a0, a1);
}

static int ctx_hook(void *a0, void *a1)
{
    return called(&g_ctx, a0, a1, (unsigned int)__builtin_return_address(0));
}

static int push_hook(void *a0, void *a1)
{
    return called(&g_push, a0, a1, (unsigned int)__builtin_return_address(0));
}

static void find(hooked *h, unsigned int text, unsigned int size)
{
    int matches = 0; /* (xmbctrl takes the first for one, the last for the other: say how many) */

    for (unsigned int a = text; a < text + size; a += 4)
        if (LW(a) == h->pattern && matches++ == 0)
            h->fn = a - h->back;
    for (unsigned int a = text + 0x50000; h->fn && a < text + size; a += 4)
        if (LW(a) == h->fn) {
            h->entry = a;
            break;
        }
    note("%s: function %08X (%d matches), table entry %08X\n", h->name, h->fn, matches, h->entry);
}

static void wrap(hooked *h, xmb_fn hook)
{
    if (!h->entry)
        return;
    h->next = (xmb_fn)LW(h->entry); /* xmbctrl's, or still the firmware's */
    if (!USER(h->next)) {
        note("%s: entry holds %08X, left alone\n", h->name, (unsigned int)h->next);
        return;
    }
    *(volatile unsigned int *)h->entry = (unsigned int)hook;
    note("%s: wrapped (it held %08X)\n", h->name, (unsigned int)h->next);
}

/* ---- the modules to dump: their records, copied while the handler may read them ---- */

static struct {
    char name[28];
    unsigned int text, text_size, data_size, bss_size, entry, gp;
    unsigned int nseg, seg[4], seg_size[4];
    unsigned int ent_top, ent_size, stub_top, stub_size;
    volatile int ready, done;
} g_mod[DUMP_MAX];
static volatile int g_nmod;

static void record(const SceModule *m)
{
    int i = g_nmod;

    if (i >= DUMP_MAX)
        return;
    strncpy(g_mod[i].name, m->modname, sizeof(g_mod[i].name) - 1);
    g_mod[i].text = m->text_addr;
    g_mod[i].text_size = m->text_size;
    g_mod[i].data_size = m->data_size;
    g_mod[i].bss_size = m->bss_size;
    g_mod[i].entry = m->entry_addr;
    g_mod[i].gp = m->gp_value;
    g_mod[i].nseg = m->nsegment < 4 ? m->nsegment : 4;
    for (unsigned int s = 0; s < g_mod[i].nseg; s++) {
        g_mod[i].seg[s] = m->segmentaddr[s];
        g_mod[i].seg_size[s] = m->segmentsize[s];
    }
    g_mod[i].ent_top = (unsigned int)m->ent_top;
    g_mod[i].ent_size = m->ent_size;
    g_mod[i].stub_top = (unsigned int)m->stub_top;
    g_mod[i].stub_size = m->stub_size;
    g_mod[i].ready = 1;
    g_nmod = i + 1;
    note("module %s: text %08X size %X, data %X, bss %X, %u segments\n", m->modname, m->text_addr,
         m->text_size, m->data_size, m->bss_size, g_mod[i].nseg);
}

static int on_module_start(SceModule *mod)
{
    int vsh = strcmp(mod->modname, "vsh_module") == 0, ret = 0;

    if (vsh || strcmp(mod->modname, "game_plugin_module") == 0)
        record(mod);
    if (vsh) {
        find(&g_ctx, mod->text_addr, mod->text_size);
        find(&g_push, mod->text_addr, mod->text_size);
    }
    if (g_previous)
        ret = g_previous(mod);
    if (vsh) {
        wrap(&g_ctx, ctx_hook);
        wrap(&g_push, push_hook);
        sctrlFlushCache();
    }
    return ret;
}

/* ---- the thread: the log to the card, the dumps ---- */

static void put(SceUID fd, const char *fmt, ...)
{
    char line[160];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = stbsp_vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    if (n > 0)
        sceIoWrite(fd, line, n > (int)sizeof(line) - 1 ? (int)sizeof(line) - 1 : n);
}

/* The import and export tables, each function as "<lib> <nid> <address>". */
static void tables(SceUID fd, int i)
{
    for (unsigned int a = g_mod[i].stub_top;
         USER(a) && a < g_mod[i].stub_top + g_mod[i].stub_size;) {
        const SceLibraryStubTable *t = (const SceLibraryStubTable *)a;

        if (t->len == 0)
            break;
        for (int k = 0; k < t->stubcount && USER(t->nidtable); k++)
            put(fd, "import %s %08X %08X\n", USER(t->libname) ? t->libname : "?", t->nidtable[k],
                (unsigned int)t->stubtable + 8 * k);
        a += t->len * 4;
    }
    for (unsigned int a = g_mod[i].ent_top; USER(a) && a < g_mod[i].ent_top + g_mod[i].ent_size;) {
        const SceLibraryEntryTable *t = (const SceLibraryEntryTable *)a;
        int n = t->stubcount + t->vstubcount;
        const unsigned int *e = t->entrytable;

        if (t->len == 0)
            break;
        for (int k = 0; k < n && USER(e); k++)
            put(fd, "export %s %08X %08X\n", USER(t->libname) ? t->libname : "syslib", e[k],
                e[n + k]);
        a += t->len * 4;
    }
}

static void dump(int i)
{
    char path[96];
    SceUID fd;

    for (unsigned int s = 0; s < g_mod[i].nseg; s++) {
        if (!USER(g_mod[i].seg[s]) || g_mod[i].seg_size[s] == 0)
            continue;
        stbsp_snprintf(path, sizeof(path), DIR "/%s.seg%u.bin", g_mod[i].name, s);
        fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
        if (fd >= 0) {
            int w = sceIoWrite(fd, (void *)g_mod[i].seg[s], g_mod[i].seg_size[s]);

            sceIoClose(fd);
            note("dump %s: %d of %u bytes\n", path, w, g_mod[i].seg_size[s]);
        }
    }
    stbsp_snprintf(path, sizeof(path), DIR "/%s.txt", g_mod[i].name);
    fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0)
        return;
    put(fd, "module %s\ntext %08X %08X\ndata_size %08X\nbss_size %08X\nentry %08X\ngp %08X\n",
        g_mod[i].name, g_mod[i].text, g_mod[i].text_size, g_mod[i].data_size, g_mod[i].bss_size,
        g_mod[i].entry, g_mod[i].gp);
    for (unsigned int s = 0; s < g_mod[i].nseg; s++)
        put(fd, "segment %u %08X %08X\n", s, g_mod[i].seg[s], g_mod[i].seg_size[s]);
    if (!strcmp(g_mod[i].name, "vsh_module"))
        put(fd, "OnXmbContextMenu %08X entry %08X\nOnXmbPush %08X entry %08X\n", g_ctx.fn,
            g_ctx.entry, g_push.fn, g_push.entry);
    tables(fd, i);
    sceIoClose(fd);
}

static int worker(SceSize args, void *argp)
{
    int written = 0, waited = 0;

    (void)args;
    (void)argp;
    sceIoMkdir(DIR, 0777);
    for (;;) {
        sceKernelDelayThread(1000 * 1000);
        waited++;
        for (int i = 0; i < g_nmod; i++)
            if (g_mod[i].ready && !g_mod[i].done) {
                g_mod[i].done = 1;
                dump(i);
            }
        if (g_len > written) { /* the new lines */
            SceUID fd =
                sceIoOpen(DIR "/probe.log", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);

            if (fd >= 0) {
                int len = g_len;

                sceIoWrite(fd, g_log + written, len - written);
                sceIoClose(fd);
                written = len;
            }
        }
        if (waited == WAIT_S && g_nmod < 2)
            note("game_plugin_module not seen in %d s\n", WAIT_S);
    }
    return 0;
}

int module_start(SceSize args, void *argp)
{
    SceUID th;

    (void)args;
    (void)argp;
    note("xmbprobe " PW_VERSION "\n");
    g_previous = sctrlHENSetStartModuleHandler(on_module_start);
    th = sceKernelCreateThread("PocketwatchProbe", worker, 0x30, 0x2000, 0, 0);
    if (th >= 0)
        sceKernelStartThread(th, 0, 0);
    return 0;
}

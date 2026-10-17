// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "complications/registry.h"
#include "pwtest.h"

/* Fakes: "a" gives two rows, "b" none, "c" one and says it changed once. */
static int g_inits, g_shutdowns, g_opens, g_order[8], g_n;

static int a_items(pw_cpl_item *out, int max)
{
    (void)max;
    out[0].title = "A0";
    out[1].title = "A1";
    out[1].enabled = 1;
    strcpy(out[1].desc, "second");
    return 2;
}

static int b_items(pw_cpl_item *out, int max)
{
    (void)out;
    (void)max;
    return 0;
}

static int c_changed_once = 1;

static int c_items(pw_cpl_item *out, int max)
{
    (void)max;
    out[0].title = "C0";
    memset(out[0].desc, 'x', sizeof(out[0].desc)); /* no terminator: the registry adds one */
    return 1;
}

static int c_changed(void)
{
    int r = c_changed_once;

    c_changed_once = 0;
    return r;
}

static int run_none(int index, const pw_cpl_env *env)
{
    (void)index;
    (void)env;
    return PW_CPL_STAY;
}

static void a_init(void)
{
    g_inits++;
    g_order[g_n++] = 'a';
}

static void c_init(void)
{
    g_inits++;
    g_order[g_n++] = 'c';
}

static void a_shutdown(void)
{
    g_shutdowns++;
    g_order[g_n++] = 'A';
}

static void c_shutdown(void)
{
    g_shutdowns++;
    g_order[g_n++] = 'C';
}

static void any_open(void)
{
    g_opens++;
}

static int c_pending = 1;

static int c_startup_pending(void)
{
    return c_pending;
}

static int c_startup(const pw_cpl_env *env)
{
    (void)env;
    c_pending = 0;
    return PW_CPL_STAY;
}

static const pw_complication A = {.id = "a",
                                  .init = a_init,
                                  .shutdown = a_shutdown,
                                  .on_open = any_open,
                                  .items = a_items,
                                  .run = run_none};
static const pw_complication B = {.id = "b", .items = b_items, .run = run_none};
static const pw_complication C = {.id = "c",
                                  .init = c_init,
                                  .shutdown = c_shutdown,
                                  .on_open = any_open,
                                  .items = c_items,
                                  .run = run_none,
                                  .changed = c_changed,
                                  .startup_pending = c_startup_pending,
                                  .startup = c_startup};
static const pw_complication *const LIST[] = {&A, &B, &C};

PW_TEST(registry_collects_rows_in_table_order)
{
    pw_hub_entry e[8];
    int n = pw_registry_collect(LIST, 3, e, 8);

    CHECK_EQ(n, 3);
    CHECK(e[0].cpl == &A && e[0].index == 0 && strcmp(e[0].item.title, "A0") == 0);
    CHECK(e[1].cpl == &A && e[1].index == 1 && strcmp(e[1].item.desc, "second") == 0);
    CHECK_EQ(e[1].item.enabled, 1);
    CHECK_EQ(e[0].item.enabled, 0);
    CHECK(e[2].cpl == &C && e[2].index == 0);
    CHECK_EQ(strlen(e[2].item.desc), (size_t)(PW_CPL_DESC_MAX - 1));
    CHECK_EQ(pw_registry_collect(LIST, 3, e, 2), 2); /* the hub's limit */
}

PW_TEST(registry_lifecycle_order_and_changes)
{
    g_inits = g_shutdowns = g_opens = g_n = 0;
    pw_registry_init(LIST, 3);
    pw_registry_open(LIST, 3);
    pw_registry_shutdown(LIST, 3);
    CHECK_EQ(g_inits, 2);
    CHECK_EQ(g_opens, 2);
    CHECK_EQ(g_shutdowns, 2);
    CHECK(g_order[0] == 'a' && g_order[1] == 'c' && g_order[2] == 'C' && g_order[3] == 'A');

    c_changed_once = 1;
    CHECK_EQ(pw_registry_changed(LIST, 3), 1);
    CHECK_EQ(pw_registry_changed(LIST, 3), 0);
}

PW_TEST(registry_finds_a_pending_startup)
{
    c_pending = 1;
    CHECK(pw_registry_startup(LIST, 3) == &C);
    C.startup(NULL);
    CHECK(pw_registry_startup(LIST, 3) == NULL);
    CHECK(pw_registry_startup(LIST, 2) == NULL); /* A and B have none */
}

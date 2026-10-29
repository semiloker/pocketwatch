// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "complications/registry.h"

int pw_registry_collect(const pw_complication *const *list, int n, pw_hub_entry *out, int max)
{
    int count = 0;

    for (int i = 0; i < n && count < max; i++) {
        pw_cpl_item items[PW_CPL_ROWS_MAX];
        int got;

        if (list[i] == 0 || list[i]->items == 0)
            continue;
        memset(items, 0, sizeof(items));
        got = list[i]->items(items, PW_CPL_ROWS_MAX);
        if (got > PW_CPL_ROWS_MAX)
            got = PW_CPL_ROWS_MAX;
        for (int k = 0; k < got && count < max; k++) {
            out[count].cpl = list[i];
            out[count].index = k;
            out[count].item = items[k];
            out[count].item.desc[PW_CPL_DESC_MAX - 1] = 0;
            count++;
        }
    }
    return count;
}

void pw_registry_init(const pw_complication *const *list, int n)
{
    for (int i = 0; i < n; i++)
        if (list[i] && list[i]->init)
            list[i]->init();
}

void pw_registry_shutdown(const pw_complication *const *list, int n)
{
    for (int i = n - 1; i >= 0; i--) /* the reverse of init */
        if (list[i] && list[i]->shutdown)
            list[i]->shutdown();
}

void pw_registry_open(const pw_complication *const *list, int n)
{
    for (int i = 0; i < n; i++)
        if (list[i] && list[i]->on_open)
            list[i]->on_open();
}

int pw_registry_changed(const pw_complication *const *list, int n)
{
    int any = 0;

    for (int i = 0; i < n; i++) /* every one is asked: changed() may update what it remembers */
        if (list[i] && list[i]->changed && list[i]->changed())
            any = 1;
    return any;
}

const pw_complication *pw_registry_startup(const pw_complication *const *list, int n)
{
    for (int i = 0; i < n; i++)
        if (list[i] && list[i]->startup && list[i]->startup_pending && list[i]->startup_pending())
            return list[i];
    return 0;
}

void pw_registry_tick(const pw_complication *const *list, int n, unsigned int now_ms)
{
    for (int i = 0; i < n; i++)
        if (list[i] && list[i]->tick)
            list[i]->tick(now_ms);
}

void pw_registry_quit(const pw_complication *const *list, int n)
{
    for (int i = 0; i < n; i++)
        if (list[i] && list[i]->on_quit)
            list[i]->on_quit();
}

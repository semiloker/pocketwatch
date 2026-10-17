// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_REGISTRY_H
#define PW_REGISTRY_H

#include "complications/complication.h"

/* The table (complications/table.c): every complication, in hub order. */
extern const pw_complication *const pw_complications[];
extern const int pw_complication_count;

/* A hub row and where it comes from. */
typedef struct {
    const pw_complication *cpl;
    int index; /* the row's index within its complication */
    pw_cpl_item item;
} pw_hub_entry;

/* Portable helpers over any list (the hub passes the table; tests pass their own). */
int pw_registry_collect(const pw_complication *const *list, int n, pw_hub_entry *out, int max);
void pw_registry_init(const pw_complication *const *list, int n);
void pw_registry_shutdown(const pw_complication *const *list, int n);
void pw_registry_open(const pw_complication *const *list, int n);
int pw_registry_changed(const pw_complication *const *list, int n); /* asks every one */

/* The first complication with a start-up action pending, or NULL. */
const pw_complication *pw_registry_startup(const pw_complication *const *list, int n);

#endif

// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_MEM_H
#define PW_MEM_H

/*
 * Big buffers come from the PSP-3000's extra RAM, never from kernel RAM (P1 has ~300 KB free
 * in games, see docs/research/m1-hardware.md). Allocate while needed, free right after.
 */

#define PW_MEM_PARTITION_EXTRA 9 /* 0x8A000000, 24 MB, free during gameplay */
/* 0x8BC00000, 4 MB. Scratch only: every suspend overwrites it with the Media Engine's eDRAM
   (the firmware's ME save area on the PSP-2000/3000), so nothing may live there across a sleep. */
#define PW_MEM_PARTITION_SPARE 11

typedef struct {
    int block; /* sysmem block UID, < 0 when empty */
    void *ptr; /* 64-byte aligned */
    unsigned int size;
} pw_mem_block;

/* Allocate size bytes (64-byte aligned) from partition 9, falling back to 11. Returns 0 or < 0. */
int pw_mem_alloc(pw_mem_block *b, const char *name, unsigned int size);
/* Allocate exactly from one partition. Returns 0 or < 0. */
int pw_mem_alloc_in(pw_mem_block *b, int partition, const char *name, unsigned int size);

/* The same from the top of the partition, so the low end stays one free block (the state's P9
   block must start at the partition's base, docs/research/m3-menu-e3.md). */
int pw_mem_alloc_high(pw_mem_block *b, int partition, const char *name, unsigned int size);

/* Largest size pw_mem_alloc_in can currently get from a partition (alignment slack removed). */
unsigned int pw_mem_max_free(int partition);

void pw_mem_free(pw_mem_block *b);

#endif

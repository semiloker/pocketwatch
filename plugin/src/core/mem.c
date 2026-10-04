// SPDX-License-Identifier: GPL-3.0-or-later
#include <pspsysmem_kernel.h>

#include "core/mem.h"

#define ALIGNMENT 64

static int alloc_from(pw_mem_block *b, int partition, const char *name, unsigned int size)
{
    SceUID block =
        sceKernelAllocPartitionMemory(partition, name, PSP_SMEM_Low, size + ALIGNMENT, NULL);
    unsigned int addr;

    if (block < 0)
        return block;
    addr = (unsigned int)sceKernelGetBlockHeadAddr(block);
    b->block = block;
    b->ptr = (void *)((addr + ALIGNMENT - 1) & ~(unsigned int)(ALIGNMENT - 1));
    b->size = size;
    return 0;
}

int pw_mem_alloc(pw_mem_block *b, const char *name, unsigned int size)
{
    int ret;

    b->block = -1;
    b->ptr = 0;
    b->size = 0;
    ret = alloc_from(b, PW_MEM_PARTITION_EXTRA, name, size);
    if (ret < 0)
        ret = alloc_from(b, PW_MEM_PARTITION_SPARE, name, size);
    return ret;
}

void pw_mem_free(pw_mem_block *b)
{
    if (b->block >= 0)
        sceKernelFreePartitionMemory(b->block);
    b->block = -1;
    b->ptr = 0;
    b->size = 0;
}

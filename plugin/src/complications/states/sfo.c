// SPDX-License-Identifier: GPL-3.0-or-later
#include <string.h>

#include "complications/states/sfo.h"

#define SFO_HEADER_SIZE 20
#define SFO_INDEX_SIZE  16

static void put_le16(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
}

static void put_le32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

int pw_sfo_build(const pw_sfo_entry *entries, int count, unsigned char *out, unsigned int cap)
{
    unsigned int key_start = SFO_HEADER_SIZE + SFO_INDEX_SIZE * (unsigned int)count;
    unsigned int keys_len = 0, data_len = 0, data_start, total;

    for (int i = 0; i < count; i++) {
        if (entries[i].len > entries[i].max_len)
            return -1;
        keys_len += (unsigned int)strlen(entries[i].key) + 1;
        data_len += entries[i].max_len;
    }
    keys_len += (4 - (key_start + keys_len) % 4) % 4; /* data table is 4-byte aligned */
    data_start = key_start + keys_len;
    total = data_start + data_len;
    if (total > cap)
        return -1;

    memset(out, 0, total);
    memcpy(out, "\0PSF", 4);
    put_le32(out + 4, 0x00000101);
    put_le32(out + 8, key_start);
    put_le32(out + 12, data_start);
    put_le32(out + 16, (unsigned int)count);

    unsigned int key_off = 0, data_off = 0;
    for (int i = 0; i < count; i++) {
        unsigned char *index = out + SFO_HEADER_SIZE + SFO_INDEX_SIZE * (unsigned int)i;
        unsigned int key_len = (unsigned int)strlen(entries[i].key) + 1;

        put_le16(index, key_off);
        put_le16(index + 2, entries[i].fmt);
        put_le32(index + 4, entries[i].len);
        put_le32(index + 8, entries[i].max_len);
        put_le32(index + 12, data_off);
        memcpy(out + key_start + key_off, entries[i].key, key_len);
        memcpy(out + data_start + data_off, entries[i].value, entries[i].len);
        key_off += key_len;
        data_off += entries[i].max_len;
    }
    return (int)total;
}

int pw_sfo_build_savedata(const pw_savedata_sfo *s, unsigned char *out, unsigned int cap,
                          unsigned int *params_offset)
{
    static const unsigned char zero_params[PW_SFO_PARAMS_SIZE];
    static const unsigned char empty_list[PW_SFO_FILE_LIST_SIZE];
    unsigned char parental[4];
    int size;

    put_le32(parental, s->parental_level);

    /* Sorted by key, with the max lengths real saves use. */
    const pw_sfo_entry entries[] = {
        {"CATEGORY", PW_SFO_FMT_UTF8, 4, "MS", 3},
        {"PARENTAL_LEVEL", PW_SFO_FMT_INT32, 4, parental, 4},
        {"SAVEDATA_DETAIL", PW_SFO_FMT_UTF8, 1024, s->detail, (unsigned int)strlen(s->detail) + 1},
        {"SAVEDATA_DIRECTORY", PW_SFO_FMT_UTF8, 64, s->directory,
         (unsigned int)strlen(s->directory) + 1},
        {"SAVEDATA_FILE_LIST", PW_SFO_FMT_UTF8_RAW, PW_SFO_FILE_LIST_SIZE,
         s->file_list ? s->file_list : empty_list, PW_SFO_FILE_LIST_SIZE},
        {"SAVEDATA_PARAMS", PW_SFO_FMT_UTF8_RAW, PW_SFO_PARAMS_SIZE, zero_params,
         PW_SFO_PARAMS_SIZE},
        {"SAVEDATA_TITLE", PW_SFO_FMT_UTF8, 128, s->savedata_title,
         (unsigned int)strlen(s->savedata_title) + 1},
        {"TITLE", PW_SFO_FMT_UTF8, 128, s->title, (unsigned int)strlen(s->title) + 1},
    };

    size = pw_sfo_build(entries, (int)(sizeof(entries) / sizeof(entries[0])), out, cap);
    if (size >= 0 && params_offset != NULL) {
        /* Data offsets are the running sum of max lengths before SAVEDATA_PARAMS. */
        unsigned int data_start = (unsigned int)out[12] | (unsigned int)out[13] << 8 |
                                  (unsigned int)out[14] << 16 | (unsigned int)out[15] << 24;
        *params_offset = data_start + 4 + 4 + 1024 + 64 + PW_SFO_FILE_LIST_SIZE;
    }
    return size;
}

static unsigned int get_le32(const unsigned char *p)
{
    return (unsigned int)p[0] | (unsigned int)p[1] << 8 | (unsigned int)p[2] << 16 |
           (unsigned int)p[3] << 24;
}

int pw_sfo_find(const unsigned char *sfo, unsigned int size, const char *key, unsigned int *offset,
                unsigned int *len)
{
    unsigned int key_start, data_start, count, key_len = (unsigned int)strlen(key) + 1;

    if (size < SFO_HEADER_SIZE || memcmp(sfo, "\0PSF", 4) != 0)
        return -1;
    key_start = get_le32(sfo + 8);
    data_start = get_le32(sfo + 12);
    count = get_le32(sfo + 16);
    if (count > (size - SFO_HEADER_SIZE) / SFO_INDEX_SIZE)
        return -1;

    for (unsigned int i = 0; i < count; i++) {
        const unsigned char *index = sfo + SFO_HEADER_SIZE + SFO_INDEX_SIZE * i;
        unsigned int k = key_start + ((unsigned int)index[0] | (unsigned int)index[1] << 8);
        unsigned int value_len = get_le32(index + 4);
        unsigned int value_off = data_start + get_le32(index + 12);

        if (k + key_len > size || memcmp(sfo + k, key, key_len) != 0)
            continue;
        if (value_off > size || value_len > size - value_off)
            return -1;
        *offset = value_off;
        *len = value_len;
        return 0;
    }
    return -1;
}

int pw_sfo_file_list_entry(unsigned char *entry, const char *name)
{
    unsigned int len = (unsigned int)strlen(name);

    if (len > 12)
        return -1;
    memset(entry, 0, PW_SFO_FILE_ENTRY_SIZE);
    memcpy(entry, name, len);
    return 0;
}

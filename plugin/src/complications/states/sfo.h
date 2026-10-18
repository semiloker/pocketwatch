// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_SFO_H
#define PW_SFO_H

/*
 * PARAM.SFO writer. Mirrors tools/sfo.py byte-for-byte (host tests compare the two), which in
 * turn reproduces every real save found on the test PSP-3000.
 */

#define PW_SFO_FMT_UTF8_RAW 0x0004 /* binary blob, not NUL-terminated */
#define PW_SFO_FMT_UTF8     0x0204 /* NUL-terminated string */
#define PW_SFO_FMT_INT32    0x0404

/* Size of the 8-key savedata PARAM.SFO (same as real saves). Multiple of 16, as hashing needs. */
#define PW_SFO_SAVEDATA_SIZE 4912

#define PW_SFO_FILE_LIST_SIZE  3168
#define PW_SFO_FILE_ENTRY_SIZE 32 /* 13-byte name, 16-byte hash, 3 bytes padding */
#define PW_SFO_PARAMS_SIZE     128

typedef struct {
    const char *key;
    unsigned short fmt;
    unsigned int max_len;
    const void *value; /* NULL: len zero bytes */
    unsigned int len;
} pw_sfo_entry;

/* entries must be sorted by key. Returns the SFO size, or -1 if it does not fit in cap. */
int pw_sfo_build(const pw_sfo_entry *entries, int count, unsigned char *out, unsigned int cap);

typedef struct {
    const char *title;              /* game title (TITLE) */
    const char *savedata_title;     /* e.g. "Save State 2" */
    const char *detail;             /* free text, may contain '\n' */
    const char *directory;          /* folder name, e.g. "PWULUS1003601" */
    const unsigned char *file_list; /* PW_SFO_FILE_LIST_SIZE bytes, or NULL for an empty list */
    unsigned int parental_level;
} pw_savedata_sfo;

/*
 * Build a savedata PARAM.SFO with zeroed SAVEDATA_PARAMS (the hash is added on the PSP).
 * out must hold PW_SFO_SAVEDATA_SIZE bytes. Returns the size, or -1 if a string is too long.
 * *params_offset receives the offset of SAVEDATA_PARAMS inside the file.
 */
int pw_sfo_build_savedata(const pw_savedata_sfo *s, unsigned char *out, unsigned int cap,
                          unsigned int *params_offset);

/* Locate a key's value inside an existing SFO. Returns 0 and fills offset/len, or -1. */
int pw_sfo_find(const unsigned char *sfo, unsigned int size, const char *key, unsigned int *offset,
                unsigned int *len);

/* Write one SAVEDATA_FILE_LIST record (name up to 12 chars, zero hash). Returns 0 or -1. */
int pw_sfo_file_list_entry(unsigned char *entry, const char *name);

#endif

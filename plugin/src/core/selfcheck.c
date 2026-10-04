// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/log.h"
#include "core/selfcheck.h"

/* Provided by the PSPSDK link script around the .lib.stub section. */
extern char __lib_stub_top[], __lib_stub_bottom[];

/* Import stub table entry, as emitted by the PSPSDK stub macros. */
typedef struct {
    const char *name;
    unsigned short version;
    unsigned short attr;
    unsigned char len; /* entry size in 32-bit words */
    unsigned char var_count;
    unsigned short func_count;
    const unsigned int *nids;
    const unsigned int *stubs; /* two instructions per function */
} StubEntry;

#define STUB_UNRESOLVED  0x0000054Cu /* syscall 0x15, written by ARK-4 for unknown NIDs */
#define STUB_PLACEHOLDER 0x03E00008u /* jr $ra: the stub was never linked at all */

int pw_selfcheck_imports(void)
{
    const char *p = __lib_stub_top;
    int checked = 0, missing = 0;

    while (p < __lib_stub_bottom) {
        const StubEntry *e = (const StubEntry *)p;

        if (e->len == 0)
            break;
        for (unsigned int i = 0; i < e->func_count; i++) {
            unsigned int insn = e->stubs[i * 2];

            checked++;
            if (insn == STUB_UNRESOLVED || insn == STUB_PLACEHOLDER) {
                pw_log("selfcheck: unresolved import %s 0x%08X", e->name, e->nids[i]);
                missing++;
            }
        }
        p += e->len * 4;
    }

    pw_log("selfcheck: %d import(s) checked, %d unresolved", checked, missing);
    return missing;
}

// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/log.h"
#include "core/selfcheck.h"
#include "core/stubinfo.h"

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

int pw_selfcheck_imports(void)
{
    const char *p = __lib_stub_top;
    int checked = 0, direct = 0, syscalls = 0, bad = 0;

    while (p < __lib_stub_bottom) {
        const StubEntry *e = (const StubEntry *)p;

        if (e->len == 0)
            break;
        for (unsigned int i = 0; i < e->func_count; i++) {
            const unsigned int *stub = e->stubs + i * 2;

            checked++;
            switch (pw_stub_classify(stub[0], stub[1])) {
            case PW_STUB_DIRECT:
                direct++;
                break;
            case PW_STUB_SYSCALL:
                syscalls++;
                break;
            case PW_STUB_UNRESOLVED:
                pw_log("selfcheck: unresolved import %s 0x%08X", e->name, e->nids[i]);
                bad++;
                break;
            default:
                pw_log("selfcheck: unexpected stub for %s 0x%08X: %08X %08X", e->name, e->nids[i],
                       stub[0], stub[1]);
                bad++;
                break;
            }
        }
        p += e->len * 4;
    }

    pw_log("selfcheck: %d import(s): %d direct, %d via syscall, %d unresolved", checked, direct,
           syscalls, bad);
    return bad;
}

// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PW_STUBINFO_H
#define PW_STUBINFO_H

/* How the module loader linked one import stub (its two instruction words). */
enum pw_stub_state {
    PW_STUB_DIRECT,     /* j target; nop          - kernel export, direct call */
    PW_STUB_SYSCALL,    /* jr $ra; syscall N      - user-mode export reached via syscall */
    PW_STUB_UNRESOLVED, /* syscall 0x15 (ARK-4: unknown NID) or the untouched jr $ra; nop */
    PW_STUB_UNKNOWN,    /* anything else */
};

enum pw_stub_state pw_stub_classify(unsigned int word0, unsigned int word1);

#endif

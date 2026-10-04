// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/stubinfo.h"

#define INSN_JR_RA         0x03E00008u
#define INSN_NOP           0x00000000u
#define SYSCALL_UNRESOLVED 0x15u
#define OPCODE(w)          ((w) >> 26)
#define IS_SYSCALL(w)      (((w) & 0xFC00003Fu) == 0x0000000Cu)
#define SYSCALL_CODE(w)    (((w) >> 6) & 0xFFFFFu)

enum pw_stub_state pw_stub_classify(unsigned int word0, unsigned int word1)
{
    if (IS_SYSCALL(word0) && SYSCALL_CODE(word0) == SYSCALL_UNRESOLVED)
        return PW_STUB_UNRESOLVED;
    if (OPCODE(word0) == 2 /* j */)
        return PW_STUB_DIRECT;
    if (word0 == INSN_JR_RA) {
        if (word1 == INSN_NOP)
            return PW_STUB_UNRESOLVED;
        if (IS_SYSCALL(word1))
            return SYSCALL_CODE(word1) == SYSCALL_UNRESOLVED ? PW_STUB_UNRESOLVED : PW_STUB_SYSCALL;
    }
    return PW_STUB_UNKNOWN;
}

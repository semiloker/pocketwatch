// SPDX-License-Identifier: GPL-3.0-or-later
#include "core/stubinfo.h"
#include "pwtest.h"

#define J(addr)    (0x08000000u | (((addr) >> 2) & 0x03FFFFFFu))
#define SYSCALL(n) (((n) << 6) | 0x0Cu)
#define JR_RA      0x03E00008u
#define NOP        0x00000000u

PW_TEST(stub_direct_jump_is_resolved)
{
    CHECK_EQ(pw_stub_classify(J(0x8804EF00), NOP), PW_STUB_DIRECT);
}

PW_TEST(stub_user_export_via_syscall_is_resolved)
{
    /* Seen on the PSP-3000 for sceRtcGetCurrentTick imported from the user library sceRtc. */
    CHECK_EQ(pw_stub_classify(JR_RA, SYSCALL(0x2104)), PW_STUB_SYSCALL);
}

PW_TEST(stub_ark_unknown_nid_is_unresolved)
{
    CHECK_EQ(pw_stub_classify(SYSCALL(0x15), NOP), PW_STUB_UNRESOLVED);
    CHECK_EQ(pw_stub_classify(JR_RA, SYSCALL(0x15)), PW_STUB_UNRESOLVED);
}

PW_TEST(stub_untouched_placeholder_is_unresolved)
{
    CHECK_EQ(pw_stub_classify(JR_RA, NOP), PW_STUB_UNRESOLVED);
}

PW_TEST(stub_garbage_is_unknown)
{
    CHECK_EQ(pw_stub_classify(0x24020001u /* li v0,1 */, NOP), PW_STUB_UNKNOWN);
}

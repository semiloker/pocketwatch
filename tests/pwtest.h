// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef PWTEST_H
#define PWTEST_H

/* Minimal host test framework: PW_TEST(name) { CHECK(...); } registers itself before main. */

#include <string.h>

typedef void (*pwt_fn)(void);

void pwt_register(const char *name, pwt_fn fn);
void pwt_failf(const char *file, int line, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

#define PW_TEST(name)                                                                              \
    static void name(void);                                                                        \
    __attribute__((constructor)) static void pwt_reg_##name(void)                                  \
    {                                                                                              \
        pwt_register(#name, name);                                                                 \
    }                                                                                              \
    static void name(void)

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond))                                                                               \
            pwt_failf(__FILE__, __LINE__, "%s", #cond);                                            \
    } while (0)

#define CHECK_EQ(a, b)                                                                             \
    do {                                                                                           \
        long long pwt_a_ = (long long)(a), pwt_b_ = (long long)(b);                                \
        if (pwt_a_ != pwt_b_)                                                                      \
            pwt_failf(__FILE__, __LINE__, "%s == %s (%lld != %lld)", #a, #b, pwt_a_, pwt_b_);      \
    } while (0)

#define CHECK_STR(a, b)                                                                            \
    do {                                                                                           \
        const char *pwt_a_ = (a), *pwt_b_ = (b);                                                   \
        if (strcmp(pwt_a_, pwt_b_) != 0)                                                           \
            pwt_failf(__FILE__, __LINE__, "%s == %s (\"%s\" != \"%s\")", #a, #b, pwt_a_, pwt_b_);  \
    } while (0)

#endif

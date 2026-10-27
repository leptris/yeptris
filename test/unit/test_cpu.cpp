/* test_cpu.cpp — detection snapshot sanity (TODO.impl/02 Phase A). */

#include <gtest/gtest.h>

#include "common/cpu.h"

TEST(Cpu, DetectsAndIsStable) {
    yep_cpu_features a = yep_cpu_detect();
    EXPECT_TRUE(a.detected);

    yep_cpu_features b = yep_cpu_detect();
    EXPECT_EQ(a.sse2, b.sse2);
    EXPECT_EQ(a.avx2, b.avx2);
    EXPECT_EQ(a.neon, b.neon);
    EXPECT_EQ(a.detected, b.detected);
}

TEST(Cpu, BaselineIsaPresent) {
    yep_cpu_features f = yep_cpu_detect();
#if defined(__x86_64__) || defined(__i386__)
    EXPECT_TRUE(f.sse2) << "SSE2 is architectural on x86-64";
#else
    EXPECT_FALSE(f.sse2) << "SSE2 reported on a non-x86 host";
#endif
#if defined(__aarch64__)
    EXPECT_TRUE(f.neon) << "NEON is architectural on AArch64";
#endif
}

TEST(Cpu, Avx2ImpliesAvx) {
    yep_cpu_features f = yep_cpu_detect();
    if (f.avx2) {
        EXPECT_TRUE(f.avx);
    }
}

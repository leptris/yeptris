/* test_float_limbs.cpp — the portable lo/hi 128-bit tier (the 32-bit
 * path), forced onto 64-bit hosts so the limb math is pinned where the
 * suite actually runs (the qemu armv7 build exercises it natively). */
#include <gtest/gtest.h>

#define YEP_FLOAT_FORCE_LIMBS 1
#include "emit/float/floatint.h"

#include <cstdint>

/* The assertions ride a noinline checker: 16-byte struct constants
 * inside the test body's cold paths trigger an Apple-linker constant-
 * pool miscompile (EXC_BAD_ACCESS at the literal load, arm64 -O2);
 * through the call boundary the same assertions link clean. */
#if defined(_MSC_VER)
#define YEP_NOINLINE __declspec(noinline)
#else
#define YEP_NOINLINE __attribute__((noinline))
#endif
static YEP_NOINLINE void expect_u128(const char* what, yep_u128 r, uint64_t lo, uint64_t hi) {
    EXPECT_EQ(r.lo, lo) << what;
    EXPECT_EQ(r.hi, hi) << what;
}

TEST(FloatLimbs, Mul64Known) {
    expect_u128("3*5", yep_u128_mul64(3, 5), 15, 0);
    expect_u128("max*max", yep_u128_mul64(0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFFFFFFFFFull),
                0x0000000000000001ull, 0xFFFFFFFFFFFFFFFEull);
    expect_u128("2^32*2^32", yep_u128_mul64(0x100000000ull, 0x100000000ull), 0, 1);
    expect_u128("0x123456789ABCDEF*10", yep_u128_mul64(0x123456789ABCDEFull, 10),
                0x0B60B60B60B60B56ull, 0);
}

TEST(FloatLimbs, ArithmeticRoundtrip) {
    yep_u128 a = yep_u128_of(0xDEADBEEFCAFEBABEull);
    yep_u128 s = yep_u128_add(a, yep_u128_of(1));
    yep_u128 back = yep_u128_sub(s, yep_u128_of(1));
    expect_u128("add/sub inverse", back, 0xDEADBEEFCAFEBABEull, 0);
    yep_u128 sh = yep_u128_shl(a, 37);
    yep_u128 shback = yep_u128_shr(sh, 37);
    expect_u128("shl/shr inverse", shback, 0xDEADBEEFCAFEBABEull, 0);
    EXPECT_EQ(yep_u128_cmp(a, a), 0);
    EXPECT_EQ(yep_u128_cmp(yep_u128_of(1), a), -1);
    EXPECT_EQ(yep_u128_cmp(a, yep_u128_of(1)), 1);
}

TEST(FloatLimbs, Fits10AndMulSmall) {
    EXPECT_EQ(yep_u128_fits10(yep_u128_max()), 0);
    EXPECT_EQ(yep_u128_fits10(yep_u128_shr(yep_u128_max(), 3)), 0); /* 2^125 * 10 > 2^128 */
    EXPECT_EQ(yep_u128_fits10(yep_u128_of(1000)), 1);
    yep_u128 m = yep_u128_mul_small(yep_u128_of(1000), 10);
    expect_u128("1000*10", m, 10000, 0);
    /* the emit loop's exact case: big-hi value, *10 with the lo carry */
    yep_u128 v = yep_u128_add(yep_u128_shl(yep_u128_of(0x1999999999999999ull), 64),
                              yep_u128_of(0x9999999999999999ull));
    EXPECT_EQ(yep_u128_fits10(v), 1);
    yep_u128 v10 = yep_u128_mul_small(v, 10);
    EXPECT_EQ(yep_u128_cmp(v10, v), 1);
}

TEST(FloatLimbs, MatchesMul64) {
    /* mul_small and the 64x64 product agree for the sampled low values */
    for (uint64_t probe : {0x1ull, 0xFFull, 0x10000ull, 0xDEADBEEFull, 0xFFFFFFFFFFFFFFFFull,
                           0x123456789ABCDEFull}) {
        yep_u128 r = yep_u128_mul64(probe, 10);
        yep_u128 m = yep_u128_mul_small(yep_u128_of(probe), 10);
        expect_u128("mul agreement", r, m.lo, m.hi);
    }
}

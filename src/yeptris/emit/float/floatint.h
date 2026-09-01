/* floatint.h — internal float-printing declarations (TODO.impl/14).
 *
 * CLEAN-ROOM IMPLEMENTATION: no third-party code is vendored (licensing
 * decision 2026-09-01). What follows implements the published interval
 * method for shortest round-trip decimal output (the Dragon4/Steele-
 * White family, described in the literature) entirely from the math:
 * find the shortest decimal that lies strictly inside the rounding
 * interval (v - w_low, v + w_high) of the binary value.
 *
 * Two tiers, one algorithm:
 *   - tier A (print.c): the same digit loop over exact 128-bit
 *     integers — covers every value whose scaled representation fits
 *     (doubles with E in [-32, 75], floats with E in [-32, 104]);
 *   - tier B (dragon.c): the same loop over a fixed-capacity limb
 *     vector — exact for everything else (extreme exponents), and the
 *     correctness oracle for tier A in tests.
 */

#ifndef YEP_FLOATINT_H
#define YEP_FLOATINT_H

#include <stdint.h>

#include "emit/float/api.h"

#if defined(__SIZEOF_INT128__)
typedef unsigned __int128 yep_u128;
#define YEP_MUL64(a, b) ((yep_u128)(uint64_t)(a) * (uint64_t)(b))
#elif defined(_MSC_VER)
/* MSVC: carry-less 64x64 via _umul128 lives in print.c directly */
#error "128-bit multiply required: port via _umul128/_umulh here"
#endif

/* The shortest-roundtrip result both tiers produce. digits[] is the
 * rounded digit sequence (no leading zero, trailing zeros kept only
 * when significant), len its length; k is the decimal exponent so the
 * value is 0.<digits> * 10^k (i.e. in [10^(k-1), 10^k)). */
typedef struct {
    char digits[24];
    int len;
    int k;
} yep_dsplit;

/* Tier B (dragon.c): exact shortest for any finite double/float bits.
 * m2/e2 is the (unified) significand/exponent: v = m2 * 2^e2 with
 * 2^mb <= m2 < 2^(mb+1) for normals, m2 < 2^mb for subnormals. */
int yep_dragon_shortest(uint64_t m2, int e2, int mb, int pow2_low, int tie_even, yep_dsplit* out);

/* Tier B exact fixed notation: digits of round_half_even(v * 10^p). */
int yep_dragon_fixed(uint64_t m2, int e2, uint32_t precision, char* buf);

/* Tier A (print.c): u128 fast tier; returns 0 when the value is out
 * of the exact-128 range and tier B must be used. */
int yep_u128_shortest(uint64_t m2, int e2, int mb, int pow2_low, int tie_even, yep_dsplit* out);

/* tie_even: whether a decimal landing exactly on a round-trip boundary
 * may be accepted — reparse ties to the even float, which is v only
 * when v's significand is even (callers pass m2 % 2 == 0). */

/* Shared rendering of a digit split into the canonical YAML form:
 * plain notation for 10^-4 <= |v| < 10^16, else d.dddde[+-]XX with a
 * leading "1.0e+20"-style fraction so the value re-parses as float. */
int yep_float_render(const yep_dsplit* ds, int negative, char* buf);

#endif /* YEP_FLOATINT_H */

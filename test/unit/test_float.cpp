/* test_float.cpp — the ryu port gates (TODO.impl/14):
 * round-trip exactness on random + boundary doubles, shortest-ness vs
 * a printf oracle, fixed-notation vs %.*f, float32 round-trip, and the
 * non-finite word contract.
 */

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "emit/float/api.h"

namespace {

uint64_t next_u64(uint64_t& s) {
    s += 0x9E3779B97F4A7C15ull;
    uint64_t z = s;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

} // namespace

TEST(Float, DoubleRoundtripRandom) {
    uint64_t s = 0xC0FFEE123456789ull;
    const int n = getenv("YEP_FLOAT_STRESS") ? 10000000 : 500000;
    char buf[64];
    for (int i = 0; i < n; i++) {
        uint64_t bits = next_u64(s);
        double d;
        memcpy(&d, &bits, 8);
        if (!std::isfinite(d)) {
            continue;
        }
        int len = yep_d2s_shortest(d, buf);
        buf[len] = '\0';
        double back = strtod(buf, NULL);
        EXPECT_EQ(memcmp(&back, &d, 8), 0) << "printed [" << buf << "] for bits " << std::hex
                                           << bits;
        if (HasFailure()) {
            return;
        }
    }
}

TEST(Float, DoubleRoundtripBoundary) {
    const double cases[] = {
        0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 0.1, 1.0 / 3.0, 2.0 / 3.0,
        4.9406564584124654e-324, /* min denormal */
        2.2250738585072009e-308, /* max denormal */
        2.2250738585072014e-308, /* min normal */
        1.7976931348623157e308,  /* max */
        1.7976931348623155e308,
        9007199254740992.0,      /* 2^53 */
        9007199254740993.0,      /* 2^53+1 (rounded on load; still roundtrips) */
        1e23, 1e-23, 1234567890123456.0, 5e-310, 123.4567890123456789,
        3.14159265358979323846264338327950288,
        6.02214076e23, 1.602176634e-19,
    };
    char buf[64];
    for (double d : cases) {
        int len = yep_d2s_shortest(d, buf);
        buf[len] = '\0';
        double back = strtod(buf, NULL);
        EXPECT_EQ(memcmp(&back, &d, 8), 0) << "[" << buf << "]";
        EXPECT_LE(len, 24);
    }
}

TEST(Float, ShortestIsShortest) {
    /* Oracle: the first precision p (1..17 significant digits) whose
     * %.{p-1}e form parses back to the same double must match the digit
     * count ryu produced — ryu is never longer than necessary. */
    uint64_t s = 0xDEADBEEFCAFEBABEull;
    char buf[64], oracle[64];
    const int n = getenv("YEP_FLOAT_STRESS") ? 1000000 : 50000;
    for (int i = 0; i < n; i++) {
        uint64_t bits = next_u64(s);
        double d;
        memcpy(&d, &bits, 8);
        if (!std::isfinite(d) || d == 0.0) {
            continue;
        }
        int len = yep_d2s_shortest(d, buf);
        buf[len] = '\0';
        /* significant digits: mantissa digits only (no exponent),
         * leading and trailing zeros stripped — "100.0" carries one
         * significant digit, "1.234e5" four */
        std::string digits;
        for (int k = 0; k < len; k++) {
            char c = buf[k];
            if (c == 'e' || c == 'E') {
                break;
            }
            if (c >= '0' && c <= '9') {
                digits += c;
            }
        }
        size_t b = digits.find_first_not_of('0');
        size_t epos = digits.find_last_not_of('0');
        int sig = (b == std::string::npos) ? 0 : (int)(epos - b + 1);
        int minimal = 18;
        for (int p = 1; p <= 17; p++) {
            snprintf(oracle, sizeof(oracle), "%.*e", p - 1, d);
            double back = strtod(oracle, NULL);
            if (memcmp(&back, &d, 8) == 0) {
                minimal = p;
                break;
            }
        }
        EXPECT_LE(sig, minimal) << "ryu [" << buf << "] longer than oracle for bits " << std::hex
                                << bits;
        if (HasFailure()) {
            return;
        }
    }
}

TEST(Float, Float32Roundtrip) {
    uint64_t s = 0x1234ABCD5678EF01ull;
    char buf[64];
    for (int i = 0; i < 200000; i++) {
        uint32_t bits = (uint32_t)(next_u64(s) >> 32);
        float f;
        memcpy(&f, &bits, 4);
        if (!std::isfinite(f)) {
            continue;
        }
        int len = yep_f2s_shortest(f, buf);
        buf[len] = '\0';
        float back = strtof(buf, NULL);
        EXPECT_EQ(memcmp(&back, &f, 4), 0) << "[" << buf << "]";
        if (HasFailure()) {
            return;
        }
    }
}

TEST(Float, FixedMatchesPrintf) {
    uint64_t s = 0xFEEDFACE0BADCAFEull;
    char buf[1200], oracle[1200];
    const uint32_t precs[] = {0, 1, 6, 9, 17};
    for (int i = 0; i < 10000; i++) {
        uint64_t bits = next_u64(s);
        double d;
        memcpy(&d, &bits, 8);
        if (!std::isfinite(d) || std::fabs(d) > 1e100) {
            continue; /* printf buffer contract for the oracle */
        }
        for (uint32_t p : precs) {
            int len = yep_d2fixed(d, p, buf);
            buf[len] = '\0';
            snprintf(oracle, sizeof(oracle), "%.*f", p, d);
            EXPECT_STREQ(buf, oracle) << "precision " << p;
            if (HasFailure()) {
                return;
            }
        }
    }
}

TEST(Float, NonfiniteWords) {
    char buf[16];
    EXPECT_EQ(yep_d2s_nonfinite(HUGE_VAL, buf), 4);
    EXPECT_EQ(std::string(buf, 4), ".inf");
    EXPECT_EQ(yep_d2s_nonfinite(-HUGE_VAL, buf), 5);
    EXPECT_EQ(std::string(buf, 5), "-.inf");
    EXPECT_EQ(yep_d2s_nonfinite(NAN, buf), 4);
    EXPECT_EQ(std::string(buf, 4), ".nan");
    EXPECT_EQ(yep_d2s_nonfinite(1.5, buf), 0);
}

TEST(Float, CommonYamlDoubles) {
    struct {
        double d;
        const char* want;
    } cases[] = {
        {1.0, "1.0"},   {-1.0, "-1.0"}, {0.5, "0.5"},    {3.0, "3.0"},
        {0.1, "0.1"},   {100.0, "100.0"}, {0.25, "0.25"}, {1e100, "1.0e+100"},
    };
    char buf[64];
    for (const auto& c : cases) {
        int len = yep_d2s_shortest(c.d, buf);
        buf[len] = '\0';
        EXPECT_STREQ(buf, c.want);
    }
}

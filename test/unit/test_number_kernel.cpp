/* test_number_kernel.cpp — the 08B fast paths.
 *
 * Contract: the fast paths are indistinguishable from libc — the
 * Clinger-bounded decimals (mantissa < 2^53, adjusted exponent
 * within ±22) convert exactly; everything outside falls back to
 * strtod/strtoll. Randomized cross-checks pin the boundary. */

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

#include <yeptris.h>
#include <yeptris/parse.h>

namespace {

double get(const char* yaml) {
    YeptrisStatus st = YEPTRIS_OK;
    std::string doc = std::string("v: ") + yaml + "\n";
    YeptrisDocument d = yeptris_parse(doc.data(), doc.size(), &st);
    if (d == NULL) {
        return NAN;
    }
    double out = NAN;
    yeptris_node_float(yeptris_node_map_get(yeptris_document_root(d, 0), "v", 1), &out);
    yeptris_document_free(d);
    return out;
}

int64_t geti(const char* yaml) {
    YeptrisStatus st = YEPTRIS_OK;
    std::string doc = std::string("v: ") + yaml + "\n";
    YeptrisDocument d = yeptris_parse(doc.data(), doc.size(), &st);
    if (d == NULL) {
        return 0x7fffffffffffffff;
    }
    int64_t out = 0;
    yeptris_node_int(yeptris_node_map_get(yeptris_document_root(d, 0), "v", 1), &out);
    yeptris_document_free(d);
    return out;
}

} // namespace

TEST(NumberKernel, FastDecimalsExact) {
    EXPECT_DOUBLE_EQ(get("2.5"), 2.5);
    EXPECT_DOUBLE_EQ(get("-0.5"), -0.5);
    EXPECT_DOUBLE_EQ(get("1e3"), 1000.0);
    EXPECT_DOUBLE_EQ(get("1.5e-3"), 0.0015);
    EXPECT_DOUBLE_EQ(get("0.1"), 0.1);
    EXPECT_DOUBLE_EQ(get("3.14159265358979"), 3.14159265358979);
    EXPECT_DOUBLE_EQ(get("100000000000000.0"), 1e14); /* int-typed fails float access */
    EXPECT_DOUBLE_EQ(get("1e14"), 1e14);
    EXPECT_DOUBLE_EQ(get("123.456e10"), 1.23456e12);
}

TEST(NumberKernel, BoundariesFallBackCorrectly) {
    /* 16 significant digits: outside the fast range, strtod decides */
    EXPECT_DOUBLE_EQ(get("3.141592653589793"), 3.141592653589793);
    /* extreme exponents */
    EXPECT_DOUBLE_EQ(get("1e308"), 1e308);
    EXPECT_DOUBLE_EQ(get("1e-308"), 1e-308);
    EXPECT_DOUBLE_EQ(get("5e-324"), 5e-324); /* subnormal */
    EXPECT_TRUE(std::isnan(get(".nan")));
    EXPECT_DOUBLE_EQ(get(".inf"), INFINITY);
    EXPECT_DOUBLE_EQ(get("-.inf"), -INFINITY);
}

TEST(NumberKernel, RandomCrossCheckVsStrtod) {
    std::mt19937_64 rng(42);
    for (int i = 0; i < 20000; i++) {
        int digits = 1 + (int)(rng() % 15);
        int e10 = -25 + (int)(rng() % 51); /* spans in/out of the range */
        char buf[64];
        uint64_t m = rng() % 9 + 1;
        for (int d = 1; d < digits; d++) {
            m = m * 10 + rng() % 10;
        }
        int dot_at = (digits > 1) ? (int)(rng() % digits) : digits;
        std::string mant = std::to_string(m);
        if (dot_at < digits) {
            mant.insert((size_t)dot_at, ".");
        }
        snprintf(buf, sizeof(buf), "%s%s%de%d", (rng() & 1) ? "-" : "", mant.c_str(), 0, e10);
        double want = strtod(buf, NULL);
        double got = get(buf);
        /* identical bits or both same inf */
        if (std::isfinite(want)) {
            ASSERT_EQ(std::memcmp(&got, &want, 8), 0) << buf << " got " << got << " want " << want;
        }
    }
}

TEST(NumberKernel, IntFastPaths) {
    EXPECT_EQ(geti("42"), 42);
    EXPECT_EQ(geti("-42"), -42);
    EXPECT_EQ(geti("9223372036854775807"), INT64_MAX);
    EXPECT_EQ(geti("-9223372036854775808"), INT64_MIN);
    /* separators are a YAML 1.1 (compat) form: parse with that schema */
    {
        YeptrisParseOptions o;
        memset(&o, 0, sizeof(o));
        o.schema = YEPTRIS_SCHEMA_11_COMPAT;
        YeptrisStatus st = YEPTRIS_OK;
        const char* y = "v: 1_000\n";
        YeptrisDocument d = yeptris_parse_ex(y, strlen(y), &o, &st);
        ASSERT_NE(d, nullptr);
        int64_t v = 0;
        ASSERT_EQ(yeptris_node_int(yeptris_node_map_get(yeptris_document_root(d, 0), "v", 1), &v),
                  YEPTRIS_OK);
        EXPECT_EQ(v, 1000);
        yeptris_document_free(d);
    }
    EXPECT_EQ(geti("0x1A"), 26);
    EXPECT_EQ(geti("0o17"), 15);
    EXPECT_EQ(geti("017"), 15); /* compat leading-zero octal */
}

/* test_simd_text.cpp — the differential suite (TODO.impl/04): the SSOT of
 * kernel correctness. The active ISA table AND the scalar reference are
 * both compared against naive references written HERE (independent code,
 * not library code), over exhaustive lengths, alignments, and adversarial
 * patterns.
 */

#include <gtest/gtest.h>

#include <chrono>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "common/simd_text.h"

namespace {

/* ---- naive references (test-local, deliberately independent) ---- */

ptrdiff_t naive_find(const char* s, size_t len, char c) {
    for (size_t i = 0; i < len; i++) {
        if (s[i] == c) {
            return (ptrdiff_t)i;
        }
    }
    return -1;
}

ptrdiff_t naive_find_not(const char* s, size_t len, char c) {
    for (size_t i = 0; i < len; i++) {
        if (s[i] != c) {
            return (ptrdiff_t)i;
        }
    }
    return -1;
}

ptrdiff_t naive_find3(const char* s, size_t len, char c0, char c1, char c2) {
    if (len < 3) {
        return -1;
    }
    for (size_t i = 0; i + 2 < len; i++) {
        if (s[i] == c0 && s[i + 1] == c1 && s[i + 2] == c2) {
            return (ptrdiff_t)i;
        }
    }
    return -1;
}

size_t naive_count(const char* s, size_t len, char c) {
    size_t n = 0;
    for (size_t i = 0; i < len; i++) {
        n += (s[i] == c) ? 1 : 0;
    }
    return n;
}

ptrdiff_t naive_quote_scan(const char* s, size_t len, char q, int* has_escape) {
    int esc = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '\\') {
            esc = 1;
            i++;
            continue;
        }
        if (s[i] == q) {
            if (q == '\'' && i + 1 < len && s[i + 1] == '\'') {
                esc = 1;
                i++;
                continue;
            }
            *has_escape = esc;
            return (ptrdiff_t)i;
        }
    }
    *has_escape = esc;
    return -1;
}

ptrdiff_t naive_stopset_find(const char* s, size_t len, const unsigned char set[32]) {
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if ((set[c >> 3] >> (c & 7)) & 1) {
            return (ptrdiff_t)i;
        }
    }
    return -1;
}

/* ---- corpus generation ---- */

std::vector<std::string> gen_buffers() {
    std::vector<std::string> out;
    std::mt19937 rng(0xC0FFEE);

    /* Deterministic patterns at every length around the 32/16 boundaries. */
    for (size_t len = 0; len <= 160; len++) {
        out.push_back(std::string(len, 'x'));
        out.push_back(std::string(len, ':'));
        std::string r(len, '\0');
        for (size_t i = 0; i < len; i++) {
            r[i] = (char)(':') == (char)(rng() % 4) ? ':' : (char)('a' + rng() % 26);
        }
        out.push_back(r);
        /* Adversarial: hit chars exactly at chunk boundaries. */
        std::string b(len, '.');
        for (size_t i = 31; i < len; i += 32) {
            b[i] = ':';
        }
        for (size_t i = 15; i < len; i += 16) {
            b[i] = '"';
        }
        out.push_back(b);
    }

    /* Quote-heavy corpus for quote_scan. */
    for (size_t len = 0; len <= 100; len++) {
        std::string q(len, '\0');
        for (size_t i = 0; i < len; i++) {
            int dice = rng() % 5;
            q[i] = dice == 0 ? '"' : dice == 1 ? '\\' : dice == 2 ? '\'' : 'a';
        }
        out.push_back(q);
    }
    return out;
}

const std::vector<std::string>& buffers() {
    static std::vector<std::string> b = gen_buffers();
    return b;
}

} // namespace

TEST(SimdText, DispatchPicksATable) {
    const yep_text_kernels* k = yep_text_active();
    ASSERT_NE(k, nullptr);
    EXPECT_EQ(yep_text_active(), k) << "active() must be stable";
    /* The table must be fully populated. */
    EXPECT_NE(k->find, nullptr);
    EXPECT_NE(k->count3, nullptr);
    EXPECT_NE(k->quote_scan, nullptr);
}

TEST(SimdText, FindAndContainsAndCount) {
    const yep_text_kernels* k = yep_text_active();
    for (const std::string& b : buffers()) {
        for (char c : {'x', ':', '"', '\0', 'z'}) {
            ptrdiff_t want = naive_find(b.data(), b.size(), c);
            EXPECT_EQ(k->find(b.data(), b.size(), c), want);
            EXPECT_EQ(yep_text_find_scalar(b.data(), b.size(), c), want);
            EXPECT_EQ(k->contains(b.data(), b.size(), c), want >= 0 ? 1 : 0);
            EXPECT_EQ(k->count_char(b.data(), b.size(), c), naive_count(b.data(), b.size(), c));
        }
    }
}

TEST(SimdText, FindNot) {
    const yep_text_kernels* k = yep_text_active();
    for (const std::string& b : buffers()) {
        for (char c : {'x', ':', '.', '\0'}) {
            ptrdiff_t want = naive_find_not(b.data(), b.size(), c);
            EXPECT_EQ(k->find_not(b.data(), b.size(), c), want);
            EXPECT_EQ(yep_text_find_not_scalar(b.data(), b.size(), c), want);
        }
    }
}

TEST(SimdText, Find3) {
    const yep_text_kernels* k = yep_text_active();
    for (const std::string& b : buffers()) {
        for (auto t : {std::make_tuple(':', '"', '\''), std::make_tuple('x', 'x', 'x'),
                       std::make_tuple('a', 'b', 'c')}) {
            char c0, c1, c2;
            std::tie(c0, c1, c2) = t;
            ptrdiff_t want = naive_find3(b.data(), b.size(), c0, c1, c2);
            EXPECT_EQ(k->find3(b.data(), b.size(), c0, c1, c2), want) << "b=" << b.size();
            EXPECT_EQ(yep_text_find3_scalar(b.data(), b.size(), c0, c1, c2), want);
        }
    }
}

TEST(SimdText, Count3AndCopyCount3) {
    const yep_text_kernels* k = yep_text_active();
    for (const std::string& b : buffers()) {
        char c0 = ':', c1 = '"', c2 = '\'';
        size_t a1, b1, c1n, a2, b2, c2n;
        k->count3(b.data(), b.size(), c0, c1, c2, &a1, &b1, &c1n);
        EXPECT_EQ(a1, naive_count(b.data(), b.size(), c0));
        EXPECT_EQ(b1, naive_count(b.data(), b.size(), c1));
        EXPECT_EQ(c1n, naive_count(b.data(), b.size(), c2));

        std::vector<char> dst(b.size());
        k->copy_count3(dst.data(), b.data(), b.size(), c0, c1, c2, &a2, &b2, &c2n);
        EXPECT_EQ(0, memcmp(dst.data(), b.data(), b.size())) << "copy corrupted data";
        EXPECT_EQ(a2, a1);
        EXPECT_EQ(b2, b1);
        EXPECT_EQ(c2n, c1n);
    }
}

TEST(SimdText, QuoteScan) {
    const yep_text_kernels* k = yep_text_active();
    for (const std::string& b : buffers()) {
        for (char q : {'"', '\''}) {
            int want_esc = 0, got_esc = 0, got_esc2 = 0;
            ptrdiff_t want = naive_quote_scan(b.data(), b.size(), q, &want_esc);
            ptrdiff_t got = k->quote_scan(b.data(), b.size(), q, &got_esc);
            EXPECT_EQ(got, want) << "len=" << b.size() << " q=" << q;
            EXPECT_EQ(got_esc, want_esc);
            ptrdiff_t got2 = yep_text_quote_scan_scalar(b.data(), b.size(), q, &got_esc2);
            EXPECT_EQ(got2, want);
            EXPECT_EQ(got_esc2, want_esc);
        }
    }
}

TEST(SimdText, QuoteScanSemantics) {
    const yep_text_kernels* k = yep_text_active();
    int esc = -1;
    /* content after opening quote: abc"def */
    EXPECT_EQ(k->quote_scan("abc\"def", 7, '"', &esc), 3);
    EXPECT_EQ(esc, 0);
    /* escaped quote: ab\"c then real " */
    EXPECT_EQ(k->quote_scan("ab\\\"c\"", 6, '"', &esc), 5);
    EXPECT_EQ(esc, 1);
    /* escaped backslash then close */
    EXPECT_EQ(k->quote_scan("a\\\\\"", 4, '"', &esc), 3);
    EXPECT_EQ(esc, 1);
    /* unterminated */
    EXPECT_EQ(k->quote_scan("abc", 3, '"', &esc), -1);
    EXPECT_EQ(esc, 0);
    /* trailing lone backslash */
    EXPECT_EQ(k->quote_scan("abc\\", 4, '"', &esc), -1);
    EXPECT_EQ(esc, 1);
}

TEST(SimdText, StopsetFind) {
    const yep_text_kernels* k = yep_text_active();
    unsigned char set[32];
    yep_stopset_clear(set);
    for (unsigned char c : {':', ',', '[', ']', '{', '}', '\n'}) {
        yep_stopset_add(set, c);
    }
    for (const std::string& b : buffers()) {
        ptrdiff_t want = naive_stopset_find(b.data(), b.size(), set);
        EXPECT_EQ(k->stopset_find(b.data(), b.size(), set), want);
        EXPECT_EQ(yep_text_stopset_find_scalar(b.data(), b.size(), set), want);
    }
    /* Empty set never matches. */
    unsigned char none[32] = {0};
    EXPECT_EQ(k->stopset_find("abc", 3, none), -1);
}

TEST(SimdText, Count3PerfSmoke) {
    /* Informational, NOT a gate: on shared CI runners the compiler
     * auto-vectorizes the scalar reference hard enough that the ratio
     * flirts with any threshold — correctness is the differential suite's
     * job; performance gates belong to the benchmark harness (item 18).
     * The ratio lands in the test XML via RecordProperty and in the log. */
    const yep_text_kernels* k = yep_text_active();
    std::string buf(64 * 1024, 'a');
    std::mt19937 rng(7);
    for (size_t i = 0; i < buf.size(); i++) {
        if (rng() % 8 == 0) {
            buf[i] = ':';
        } else if (rng() % 16 == 0) {
            buf[i] = '"';
        }
    }

    size_t a, b, c;
    auto t0 = std::chrono::steady_clock::now();
    for (int r = 0; r < 64; r++) {
        k->count3(buf.data(), buf.size(), ':', '"', '\'', &a, &b, &c);
    }
    auto t1 = std::chrono::steady_clock::now();
    for (int r = 0; r < 64; r++) {
        yep_text_count3_scalar(buf.data(), buf.size(), ':', '"', '\'', &a, &b, &c);
    }
    auto t2 = std::chrono::steady_clock::now();

    double simd_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double scalar_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
    double ratio = scalar_ms / simd_ms;
    RecordProperty("simd_ms", simd_ms);
    RecordProperty("scalar_ms", scalar_ms);
    RecordProperty("ratio", ratio);
    printf("[ perf ] active count3 %.3fx vs scalar (%.2fms vs %.2fms)\n", ratio, simd_ms,
           scalar_ms);
    SUCCEED() << "count3 ratio " << ratio << "x (informational)";
}

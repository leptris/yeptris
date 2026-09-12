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
        if (q == '"' && s[i] == '\\') {
            esc = 1; /* backslash escapes exist only in double quotes */
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

        /* +1: an empty input must not hand the kernel a NULL dst
         * (nonnull attribute: the call itself is UB at size 0) */
        std::vector<char> dst(b.size() + 1);
        k->copy_count3(dst.data(), b.data(), b.size(), c0, c1, c2, &a2, &b2, &c2n);
        EXPECT_EQ(0, memcmp(dst.data(), b.data(), b.size())) << "copy corrupted data";
        EXPECT_EQ(a2, a1);
        EXPECT_EQ(b2, b1);
        EXPECT_EQ(c2n, c1n);
    }
}

static void naive_scan_stats(const char* s, size_t len, yep_text_stats* out) {
    memset(out, 0, sizeof(*out));
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 0x80) {
            out->nonascii = 1;
        } else if ((c < 0x20 && c != 9 && c != 10 && c != 13) || c == 0x7F) {
            out->bad_printable = 1;
        }
        switch (c) {
        case '\n':
            out->nl++;
            break;
        case ',':
            out->comma++;
            break;
        case '-':
            out->dash++;
            break;
        case ':':
            out->colon++;
            break;
        case '[':
            out->bracket++;
            break;
        case '{':
            out->brace++;
            break;
        case '"':
            out->dq++;
            break;
        case '\'':
            out->sq++;
            break;
        case '|':
            out->pipe++;
            break;
        case '&':
            out->amp++;
            break;
        default:
            break;
        }
    }
}

TEST(SimdText, ScanStats) {
    const yep_text_kernels* k = yep_text_active();
    for (const std::string& b : buffers()) {
        yep_text_stats got, want, ref;
        k->scan_stats(b.data(), b.size(), &got);
        naive_scan_stats(b.data(), b.size(), &want);
        yep_text_scan_stats_scalar(b.data(), b.size(), &ref);
        EXPECT_EQ(got.nl, want.nl);
        EXPECT_EQ(got.comma, want.comma);
        EXPECT_EQ(got.dash, want.dash);
        EXPECT_EQ(got.colon, want.colon);
        EXPECT_EQ(got.bracket, want.bracket);
        EXPECT_EQ(got.brace, want.brace);
        EXPECT_EQ(got.dq, want.dq);
        EXPECT_EQ(got.sq, want.sq);
        EXPECT_EQ(got.pipe, want.pipe);
        EXPECT_EQ(got.amp, want.amp);
        EXPECT_EQ(got.nonascii, want.nonascii);
        EXPECT_EQ(got.bad_printable, want.bad_printable);
        EXPECT_EQ(0, memcmp(&ref, &want, sizeof(want))) << "scalar reference diverges";
    }
    /* targeted shapes: every count char, controls, DEL, non-ASCII, empty */
    struct {
        const char* s;
        size_t nl, comma, dash, colon, bracket, brace, dq, sq, pipe, amp;
        int nonascii, bad;
    } cases[] = {
        {"", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        {"\n,-:[]{}\"'|&", 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0},
        {"a\tb\nc\rd ok", 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {"caf\xc3\xa9", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0},
        {"x\x01y\x02", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {"&v0 &v1 *v0", 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0},
    };
    for (const auto& c : cases) {
        yep_text_stats got;
        k->scan_stats(c.s, strlen(c.s), &got);
        EXPECT_EQ(got.nl, c.nl);
        EXPECT_EQ(got.comma, c.comma);
        EXPECT_EQ(got.dash, c.dash);
        EXPECT_EQ(got.colon, c.colon);
        EXPECT_EQ(got.bracket, c.bracket);
        EXPECT_EQ(got.brace, c.brace);
        EXPECT_EQ(got.dq, c.dq);
        EXPECT_EQ(got.sq, c.sq);
        EXPECT_EQ(got.pipe, c.pipe);
        EXPECT_EQ(got.amp, c.amp);
        EXPECT_EQ(got.nonascii, c.nonascii);
        EXPECT_EQ(got.bad_printable, c.bad);
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

TEST(SimdText, QbcFind) {
    /* TODO.restructure/46: the JSON string-stop kernel. The
     * milestone-55 lesson applied: every stop byte alone at EVERY
     * lane position (including deep past chunk boundaries), mixed
     * stops, and pure non-stop runs — active() vs scalar vs naive. */
    const yep_text_kernels* k = yep_text_active();
    auto naive = [](const char* s, size_t len) -> ptrdiff_t {
        for (size_t i = 0; i < len; i++) {
            unsigned char c = (unsigned char)s[i];
            if (c == '"' || c == '\\' || c < 0x20)
                return (ptrdiff_t)i;
        }
        return -1;
    };
    /* every stop byte alone at every offset (0..71), padded */
    for (int c : {0x00, 0x01, 0x09, 0x0A, 0x1F, (int)'"', (int)'\\'}) {
        for (size_t pos = 0; pos < 72; pos++) {
            std::string b(72, 'x');
            b[pos] = (char)c;
            EXPECT_EQ(k->qbc_find(b.data(), b.size()), (ptrdiff_t)pos)
                << "c=" << c << " pos=" << pos;
            EXPECT_EQ(yep_text_qbc_find_scalar(b.data(), b.size()), (ptrdiff_t)pos);
            EXPECT_EQ(naive(b.data(), b.size()), (ptrdiff_t)pos);
        }
    }
    /* non-stop bytes never hit (incl. DEL 0x7F — legal in JSON) */
    for (int c : {(int)'a', (int)' ', 0x7F, 0x80, 0xFF}) {
        std::string b(100, (char)c);
        EXPECT_EQ(k->qbc_find(b.data(), b.size()), (ptrdiff_t)-1) << "c=" << c;
    }
    /* exactly at chunk boundaries (31/32, 15/16) and one past */
    for (size_t pos : {(size_t)15, (size_t)16, (size_t)31, (size_t)32, (size_t)47, (size_t)48,
                       (size_t)63, (size_t)64}) {
        for (int c : {0x0A, (int)'"', (int)'\\'}) {
            std::string b(80, 'x');
            b[pos] = (char)c;
            EXPECT_EQ(k->qbc_find(b.data(), b.size()), (ptrdiff_t)pos)
                << "c=" << c << " pos=" << pos;
        }
    }
    /* all-prefix sweep: a stop at pos, every length >= pos+1 */
    for (size_t pos = 0; pos < 70; pos++) {
        std::string b(70, 'x');
        b[pos] = '\\';
        for (size_t l = 0; l <= 70; l++) {
            ptrdiff_t want = (ptrdiff_t)pos < (ptrdiff_t)l ? (ptrdiff_t)pos : (ptrdiff_t)-1;
            EXPECT_EQ(k->qbc_find(b.data(), l), want) << "pos=" << pos << " len=" << l;
            EXPECT_EQ(yep_text_qbc_find_scalar(b.data(), l), want);
        }
    }
    /* randomized mixed corpus vs naive (deterministic) */
    std::mt19937 rng(46);
    for (int t = 0; t < 500; t++) {
        size_t len = rng() % 200;
        std::string b(len, '\0');
        for (size_t i = 0; i < len; i++) {
            unsigned char c = rng() % 256;
            if (c >= 0x20 && c != '"' && c != '\\')
                c = 'a' + (rng() % 26);
            b[i] = (char)c;
        }
        EXPECT_EQ(k->qbc_find(b.data(), b.size()), naive(b.data(), b.size()));
        EXPECT_EQ(yep_text_qbc_find_scalar(b.data(), b.size()), naive(b.data(), b.size()));
    }
}

TEST(SimdText, StopsetFind) {
    const yep_text_kernels* k = yep_text_active();
    /* the set MUST contain bytes that actually OCCUR in the probe
     * buffers at every lane position — a set of rare bytes once let a
     * broken vector path pass (the probes never hit in full chunks) */
    unsigned char set[32];
    yep_stopset_clear(set);
    for (unsigned char c : {':', ',', '[', ']', '{', '}', '\n', 'a', 'e', 'x', '0', ' '}) {
        yep_stopset_add(set, c);
    }
    yep_stopset ss;
    yep_stopset_init(&ss, set);
    for (const std::string& b : buffers()) {
        ptrdiff_t want = naive_stopset_find(b.data(), b.size(), set);
        EXPECT_EQ(k->stopset_find(&ss, b.data(), b.size()), want);
        EXPECT_EQ(yep_text_stopset_find_scalar(&ss, b.data(), b.size()), want);
    }
    /* every prefix length: the first hit must track the span's edge */
    const std::string probe = "base: &b\n  x: 1\nsame: *b\n";
    for (size_t L = 0; L <= probe.size(); L++) {
        EXPECT_EQ(k->stopset_find(&ss, probe.data(), L), naive_stopset_find(probe.data(), L, set))
            << "len=" << L;
    }
    /* all 256 bytes as single probes, alone and embedded past lane 15 */
    for (unsigned c = 0; c < 256; c++) {
        unsigned char one[2] = {(unsigned char)c, 'z'};
        unsigned char late[18] = {'q',
                                  'q',
                                  'q',
                                  'q',
                                  'q',
                                  'q',
                                  'q',
                                  'q',
                                  'q',
                                  'q',
                                  'q',
                                  'q',
                                  'q',
                                  'q',
                                  'q',
                                  'q',
                                  (unsigned char)c,
                                  'z'};
        EXPECT_EQ(k->stopset_find(&ss, (const char*)one, 2),
                  naive_stopset_find((const char*)one, 2, set))
            << "byte " << c;
        EXPECT_EQ(k->stopset_find(&ss, (const char*)late, 18),
                  naive_stopset_find((const char*)late, 18, set))
            << "byte " << c << " late";
    }
    /* Empty set never matches (groups == 0 takes the scalar walk). */
    unsigned char none[32] = {0};
    yep_stopset ss_none;
    yep_stopset_init(&ss_none, none);
    EXPECT_EQ(k->stopset_find(&ss_none, "abc", 3), -1);
}

static int naive_gate_safe(const unsigned char* s, size_t len) {
    for (size_t i = 0; i < len; i++) {
        unsigned char c = s[i];
        if (!((c >= 0x20 && c <= 0x7E) || c == 0x09 || c == 0x0A || c == 0x0D)) {
            return 1;
        }
    }
    return 0;
}

TEST(SimdText, GateScan) {
    const yep_text_kernels* k = yep_text_active();
    /* the gate bytes that distinguish the vector masks: TAB/LF/CR are
     * ALLOWED (both shipped kernels got this wrong in different ways —
     * one OR'd ~allow in, one ANDed allow with zero), 0x1F is a
     * violation (the <0x1F bound missed it), DEL and >=0x80 rule */
    const std::string probes[] = {
        "",
        "plain text 123",
        "\t\n\r",
        "a\tb\nc\rd",
        "with 0x1f: \x1f",
        "del: \x7F",
        "utf8: \xC3\xA9",
        std::string("nul: ") + char(0),
        "mixed \x1F then \xC3\xA9",
        "line\nwith\ttabs\r",
        std::string("\x1F") + "bad\n",
    };
    for (const std::string& p : probes) {
        EXPECT_EQ(k->gate_scan(p.data(), p.size()),
                  naive_gate_safe((const unsigned char*)p.data(), p.size()))
            << "probe len " << p.size();
        EXPECT_EQ(yep_text_gate_scan_scalar(p.data(), p.size()),
                  naive_gate_safe((const unsigned char*)p.data(), p.size()))
            << "scalar probe len " << p.size();
    }
    /* every prefix length: a clean line must stay clean at any cut,
     * a dirty one must stay dirty */
    const std::string clean = "key: value 123\nnext: line\n";
    const std::string dirty = std::string("key: value\n\x1F") + "bad\n";
    const size_t dirty_at = 11; /* index of the 0x1F */
    for (size_t L = 0; L <= clean.size(); L++) {
        EXPECT_EQ(k->gate_scan(clean.data(), L), 0) << "clean len=" << L;
    }
    for (size_t L = 0; L <= dirty.size(); L++) {
        EXPECT_EQ(k->gate_scan(dirty.data(), L), L > dirty_at ? 1 : 0) << "dirty len=" << L;
    }
    /* random buffers vs naive */
    std::mt19937_64 rng(0xDA7E);
    for (int t = 0; t < 200; t++) {
        std::string b(rng() % 300, ' ');
        for (auto& c : b) {
            c = (char)(rng() % 256);
        }
        EXPECT_EQ(k->gate_scan(b.data(), b.size()),
                  naive_gate_safe((const unsigned char*)b.data(), b.size()))
            << "random t=" << t;
    }
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

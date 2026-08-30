/* test_encoding.cpp — encoding front-end (TODO.impl/05): BOM sniffing,
 * UTF-8 validation (differential against a naive test-local decoder over
 * the full 2-byte space), and transcode round-trips + ill-formed cases.
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "encoding/encoding.h"
#include "memory/allocator.h"

TEST(Bom, DetectsAllMarks) {
    struct {
        std::vector<unsigned char> bytes;
        yep_encoding enc;
        size_t consumed;
    } cases[] = {
        {{0xEF, 0xBB, 0xBF, 'a'}, YEP_ENC_UTF8, 3},
        {{0xFF, 0xFE, 0x00, 0x00, 'a'}, YEP_ENC_UTF32LE, 4},
        {{0x00, 0x00, 0xFE, 0xFF, 'a'}, YEP_ENC_UTF32BE, 4},
        {{0xFF, 0xFE, 'a', 'b'}, YEP_ENC_UTF16LE, 2},
        {{0xFE, 0xFF, 'a', 'b'}, YEP_ENC_UTF16BE, 2},
    };
    for (const auto& c : cases) {
        yep_encoding e = YEP_ENC_UNKNOWN;
        size_t n = yep_bom_sniff(c.bytes.data(), c.bytes.size(), &e);
        EXPECT_EQ(e, c.enc);
        EXPECT_EQ(n, c.consumed);
    }
}

TEST(Bom, PartialAndAbsentMarks) {
    yep_encoding e;
    EXPECT_EQ(yep_bom_sniff(NULL, 0, &e), 0u);
    EXPECT_EQ(e, YEP_ENC_UNKNOWN);

    EXPECT_EQ(yep_bom_sniff((const unsigned char*)"\xEF", 1, &e), 0u);
    EXPECT_EQ(yep_bom_sniff((const unsigned char*)"\xEF\xBB", 2, &e), 0u);
    EXPECT_EQ(yep_bom_sniff((const unsigned char*)"\xFF", 1, &e), 0u);
    /* FF FE with only one trailing zero byte is UTF-16LE (not 32LE). */
    EXPECT_EQ(yep_bom_sniff((const unsigned char*)"\xFF\xFE\x00", 3, &e), 2u);
    EXPECT_EQ(e, YEP_ENC_UTF16LE);

    EXPECT_EQ(yep_bom_sniff((const unsigned char*)"plain", 5, &e), 0u);
    EXPECT_EQ(e, YEP_ENC_UNKNOWN);
    EXPECT_EQ(yep_bom_sniff((const unsigned char*)"plain", 5, NULL), 0u); /* NULL out ok */
}

namespace {

/* Naive reference decoder: returns offset of first invalid byte, or -1. */
ptrdiff_t naive_utf8_check(const unsigned char* p, size_t len) {
    size_t i = 0;
    while (i < len) {
        unsigned char b = p[i];
        uint32_t cp;
        int need;
        if (b < 0x80) {
            cp = b;
            need = 0;
        } else if ((b & 0xE0) == 0xC0) {
            cp = b & 0x1F;
            need = 1;
        } else if ((b & 0xF0) == 0xE0) {
            cp = b & 0x0F;
            need = 2;
        } else if ((b & 0xF8) == 0xF0) {
            cp = b & 0x07;
            need = 3;
        } else {
            return (ptrdiff_t)i;
        }
        if (i + (size_t)need >= len) {
            return (ptrdiff_t)i; /* truncated */
        }
        for (int k = 1; k <= need; k++) {
            if ((p[i + (size_t)k] & 0xC0) != 0x80) {
                return (ptrdiff_t)(i + (size_t)k);
            }
            cp = (cp << 6) | (p[i + (size_t)k] & 0x3F);
        }
        /* Overlong / surrogate / range checks. */
        if ((need == 1 && cp < 0x80) || (need == 2 && cp < 0x800) || (need == 3 && cp < 0x10000) ||
            cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
            return (ptrdiff_t)i;
        }
        i += (size_t)need + 1;
    }
    return -1;
}

} // namespace

TEST(Utf8Validate, KnownGoodAndBad) {
    struct {
        std::string bytes;
        bool valid;
        size_t err_at;
    } cases[] = {
        {"", true, 0},
        {"ascii", true, 0},
        {"\xC3\xA9", true, 0},         /* é */
        {"\xE2\x82\xAC", true, 0},     /* € */
        {"\xF0\x9F\x98\x80", true, 0}, /* 😀 */
        {"\x7F", true, 0},
        {"\xC0\x80", false, 0}, /* overlong NUL — invalid starter */
        {"\xC1\xBF", false, 0},
        {"\xE0\x80\x80", false, 1},     /* overlong */
        {"\xED\xA0\x80", false, 1},     /* surrogate D800 */
        {"\xED\xBF\xBF", false, 1},     /* DFFF */
        {"\xF4\x90\x80\x80", false, 1}, /* > U+10FFFF */
        {"\xF5\x80\x80\x80", false, 0}, /* invalid starter */
        {"\xFF", false, 0},
        {"\x80", false, 0},     /* bare continuation */
        {"\xC2", false, 0},     /* truncated */
        {"\xC2\x41", false, 1}, /* bad continuation */
        {"\xE1\x80", false, 0}, /* truncated 3-byte */
        {"a\xC3\xA9"
         "b\xF0\x9F\x98\x80",
         true, 0},            /* 'b' must not merge into the escape */
        {"ab\xC3", false, 2}, /* truncation mid-buffer */
    };
    for (const auto& c : cases) {
        size_t err = 9999;
        int got = yep_utf8_validate((const unsigned char*)c.bytes.data(), c.bytes.size(), &err);
        EXPECT_EQ(!!got, c.valid) << "input starts with 0x" << std::hex
                                  << (c.bytes.empty() ? 0 : (unsigned char)c.bytes[0]);
        if (!c.valid) {
            /* The first byte of an invalid sequence is reported. */
            EXPECT_EQ(err, c.err_at);
        }
    }
}

TEST(Utf8Validate, ExhaustiveTwoByteSpaceAgainstNaive) {
    /* Every possible 2-byte input: our result must equal the naive
     * decoder's verdict (valid-2-byte, valid-ascii+something, or invalid
     * with matching error offset semantics). */
    unsigned char buf[2];
    for (int a = 0; a < 256; a++) {
        for (int b = 0; b < 256; b++) {
            buf[0] = (unsigned char)a;
            buf[1] = (unsigned char)b;
            ptrdiff_t want = naive_utf8_check(buf, 2);
            size_t err = 9999;
            int got = yep_utf8_validate(buf, 2, &err);
            if (want < 0) {
                EXPECT_EQ(got, 1) << "a=" << a << " b=" << b;
            } else {
                EXPECT_EQ(got, 0) << "a=" << a << " b=" << b;
            }
        }
    }
}

TEST(Utf8Validate, HighByteAnywhereInAsciiFastPath) {
    /* SWAR fast path must stop exactly at the high byte. The multibyte
     * sequence must be contiguous: C3 at pos, A9 at pos+1. */
    for (size_t pos = 0; pos + 1 < 24; pos++) {
        std::string s(24, 'x');
        s[pos] = (char)0xC3;
        s[pos + 1] = (char)0xA9;
        size_t err = 9999;
        EXPECT_EQ(yep_utf8_validate((const unsigned char*)s.data(), s.size(), &err), 1)
            << "pos=" << pos;

        /* Break the continuation: error reported at the byte after. */
        std::string bad = s;
        bad[pos + 1] = '!';
        EXPECT_EQ(yep_utf8_validate((const unsigned char*)bad.data(), bad.size(), &err), 0);
        EXPECT_EQ(err, pos + 1);
    }
}

TEST(Utf8Validate, RandomBuffersAgainstNaive) {
    std::mt19937 rng(42);
    for (int round = 0; round < 2000; round++) {
        size_t len = rng() % 40;
        std::string s(len, '\0');
        for (size_t i = 0; i < len; i++) {
            s[i] = (char)(rng() % 256);
        }
        ptrdiff_t want = naive_utf8_check((const unsigned char*)s.data(), s.size());
        int got = yep_utf8_validate((const unsigned char*)s.data(), s.size(), NULL);
        EXPECT_EQ(!!got, want < 0) << "round " << round << " len " << len;
    }
}

TEST(Transcode, Utf16RoundTripAllEndians) {
    const yep_allocator* sys = yep_system_allocator();
    /* Mixed BMP + astral: é (U+00E9), € (U+20AC), 😀 (U+1F600), A */
    const uint32_t cps[] = {0x0041, 0x00E9, 0x20AC, 0x1F600, 0x0042};

    for (yep_encoding enc : {YEP_ENC_UTF16LE, YEP_ENC_UTF16BE}) {
        std::vector<unsigned char> wide;
        for (uint32_t cp : cps) {
            if (cp >= 0x10000) {
                uint16_t hi = (uint16_t)(0xD800 + ((cp - 0x10000) >> 10));
                uint16_t lo = (uint16_t)(0xDC00 + ((cp - 0x10000) & 0x3FF));
                if (enc == YEP_ENC_UTF16LE) {
                    wide.push_back((unsigned char)(hi & 0xFF));
                    wide.push_back((unsigned char)(hi >> 8));
                    wide.push_back((unsigned char)(lo & 0xFF));
                    wide.push_back((unsigned char)(lo >> 8));
                } else {
                    wide.push_back((unsigned char)(hi >> 8));
                    wide.push_back((unsigned char)(hi & 0xFF));
                    wide.push_back((unsigned char)(lo >> 8));
                    wide.push_back((unsigned char)(lo & 0xFF));
                }
            } else {
                uint16_t u = (uint16_t)cp;
                if (enc == YEP_ENC_UTF16LE) {
                    wide.push_back((unsigned char)(u & 0xFF));
                    wide.push_back((unsigned char)(u >> 8));
                } else {
                    wide.push_back((unsigned char)(u >> 8));
                    wide.push_back((unsigned char)(u & 0xFF));
                }
            }
        }

        unsigned char* out = NULL;
        size_t out_len = 0, err = 0;
        ASSERT_EQ(yep_transcode_to_utf8(sys, enc, wide.data(), wide.size(), &out, &out_len, &err),
                  0);
        ASSERT_NE(out, nullptr);

        /* Expected UTF-8 assembled independently. */
        std::string want = "A\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80"
                           "B";
        EXPECT_EQ(out_len, want.size());
        EXPECT_EQ(0, memcmp(out, want.data(), out_len));
        yep_free(sys, out);
    }
}

TEST(Transcode, Utf32BothEndians) {
    const yep_allocator* sys = yep_system_allocator();
    uint32_t cps[] = {'x', 0x00E9, 0x1F600};
    for (yep_encoding enc : {YEP_ENC_UTF32LE, YEP_ENC_UTF32BE}) {
        std::vector<unsigned char> wide;
        for (uint32_t cp : cps) {
            unsigned char b[4] = {(unsigned char)(cp >> 24), (unsigned char)(cp >> 16),
                                  (unsigned char)(cp >> 8), (unsigned char)cp};
            if (enc == YEP_ENC_UTF32LE) {
                wide.push_back(b[3]);
                wide.push_back(b[2]);
                wide.push_back(b[1]);
                wide.push_back(b[0]);
            } else {
                wide.push_back(b[0]);
                wide.push_back(b[1]);
                wide.push_back(b[2]);
                wide.push_back(b[3]);
            }
        }
        unsigned char* out = NULL;
        size_t out_len = 0, err = 0;
        ASSERT_EQ(yep_transcode_to_utf8(sys, enc, wide.data(), wide.size(), &out, &out_len, &err),
                  0);
        std::string want = "x\xC3\xA9\xF0\x9F\x98\x80";
        EXPECT_EQ(out_len, want.size());
        EXPECT_EQ(0, memcmp(out, want.data(), out_len));
        yep_free(sys, out);
    }
}

TEST(Transcode, IllFormedInputs) {
    const yep_allocator* sys = yep_system_allocator();
    unsigned char* out = NULL;
    size_t out_len = 0, err = 0;

    /* Lone high surrogate at the end (LE). */
    unsigned char lone_hi[] = {'a', 0, 0x00, 0xD8};
    EXPECT_EQ(yep_transcode_to_utf8(sys, YEP_ENC_UTF16LE, lone_hi, 4, &out, &out_len, &err), -2);
    EXPECT_EQ(err, 2u);

    /* High surrogate followed by non-surrogate. */
    unsigned char bad_pair[] = {'a', 0, 0x00, 0xD8, 'b', 0};
    EXPECT_EQ(yep_transcode_to_utf8(sys, YEP_ENC_UTF16LE, bad_pair, 6, &out, &out_len, &err), -2);

    /* Lone low surrogate. */
    unsigned char lone_lo[] = {0x00, 0xDC, 'a', 0};
    EXPECT_EQ(yep_transcode_to_utf8(sys, YEP_ENC_UTF16LE, lone_lo, 4, &out, &out_len, &err), -2);

    /* Odd length. */
    unsigned char odd[] = {'a', 0, 'b'};
    EXPECT_EQ(yep_transcode_to_utf8(sys, YEP_ENC_UTF16LE, odd, 3, &out, &out_len, &err), -2);

    /* UTF-32 above U+10FFFF and surrogate. */
    unsigned char over[] = {0x00, 0x00, 0x01, 0x11};
    EXPECT_EQ(yep_transcode_to_utf8(sys, YEP_ENC_UTF32LE, over, 4, &out, &out_len, &err), -2);
    unsigned char surr[] = {0x00, 0x00, 0xD8, 0x00};
    EXPECT_EQ(yep_transcode_to_utf8(sys, YEP_ENC_UTF32LE, surr, 4, &out, &out_len, &err), -2);

    /* Not a transcoding source. */
    unsigned char ascii[] = "abc";
    EXPECT_EQ(yep_transcode_to_utf8(sys, YEP_ENC_UTF8, ascii, 3, &out, &out_len, &err), -2);

    /* Empty input is fine. */
    EXPECT_EQ(yep_transcode_to_utf8(sys, YEP_ENC_UTF16LE, NULL, 0, &out, &out_len, &err), 0);
    EXPECT_EQ(out_len, 0u);
}

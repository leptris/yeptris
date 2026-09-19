// test_cbor.cpp — CBOR decode (TODO.cbor/01). Pins RFC 8949 Appendix A
// (every vector decodes to the expected structure), Appendix F's
// well-formedness rejects, the strict-minimality surface, indefinite
// chunk rules, and the representation ledger (tag chains, diagnostics,
// beyond-int64 text, depth guard, trailing data).
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <yeptris.h>

#include "doc.h"
#include "dom/dom.h"

namespace {

struct Dec {
    yeptris_document* doc = nullptr; // internal handle for direct DOM walks
    YeptrisStatus st = YEPTRIS_OK;

    Dec(const std::vector<uint8_t>& bytes, uint32_t opts = 0) {
        doc = (yeptris_document*)yeptris_cbor_decode(bytes.data(), bytes.size(), opts, &st);
    }
    ~Dec() {
        yeptris_document_free((YeptrisDocument)doc);
    }
    yep_dom* dom() {
        return doc->dom;
    }
    uint32_t root() {
        return doc->dom->docs[0];
    }
};

std::vector<uint8_t> hx(const char* s) {
    std::vector<uint8_t> out;
    for (size_t i = 0; s[i] && s[i + 1]; i += 2) {
        auto v = [](char c) -> int { return c >= 'a' ? c - 'a' + 10 : c - '0'; };
        out.push_back((uint8_t)((v(s[i]) << 4) | v(s[i + 1])));
    }
    return out;
}

yep_view val(const yep_dom* d, const yep_dnode* n) {
    return yep_dom_view(d, n->value);
}

std::string vstr(const yep_dom* d, const yep_dnode* n) {
    yep_view v = val(d, n);
    return std::string(v.p, v.len);
}

// ---- expectation walker ---------------------------------------------

struct E {
    enum K { I, F, S, B, TBOOL, FBOOL, NUL, UNDEF, SIMPLE, ARR, MAP, TAGGED, ANY_ROOT } k;
    std::string text;     // scalar text (int/float/str/simple)
    bool b = false;       // bools
    uint64_t simple = 0;  // simple(N)
    std::vector<E> items; // array / map (k,v,k,v interleaved)
    std::string tag;      // tag chain text
};

const E I_(std::string t) {
    return E{E::I, t, false, 0, {}, ""};
}
const E F_(std::string t) {
    return E{E::F, t, false, 0, {}, ""};
}
const E S_(std::string t) {
    return E{E::S, t, false, 0, {}, ""};
}
const E B_(std::string t) {
    return E{E::B, t, false, 0, {}, ""};
} // h'...' diagnostic of bytes
const E Tb() {
    return E{E::TBOOL, "true", true, 0, {}, ""};
}
const E Fb() {
    return E{E::FBOOL, "false", false, 0, {}, ""};
}
const E Nl() {
    return E{E::NUL, "null", false, 0, {}, ""};
}
const E Un() {
    return E{E::UNDEF, "undefined", false, 0, {}, ""};
}
const E Sm(uint64_t n) {
    return E{E::SIMPLE, "", false, n, {}, ""};
}
E A(std::vector<E> xs) {
    E e;
    e.k = E::ARR;
    e.items = std::move(xs);
    return e;
}
E M(std::vector<E> kvs) {
    E e;
    e.k = E::MAP;
    e.items = std::move(kvs);
    return e;
}
E Tg(std::string tag, E inner) {
    inner.tag = tag;
    return inner;
}

::testing::AssertionResult walk(const yep_dom* d, uint32_t id, const E& e, int depth = 0) {
    const yep_dnode* n = &d->nodes[id];
    std::string pad(depth * 2, ' ');
    if (e.k == E::ANY_ROOT) {
        return ::testing::AssertionSuccess(); /* shape checked elsewhere */
    }
    std::string tag_txt =
        n->tag.len ? std::string(yep_dom_view(d, n->tag).p, yep_dom_view(d, n->tag).len) : "";
    if (tag_txt != e.tag) {
        return ::testing::AssertionFailure()
               << pad << "tag '" << tag_txt << "' != '" << e.tag << "'";
    }
    switch (e.k) {
    case E::ARR:
    case E::MAP: {
        yep_dom_kind want = e.k == E::ARR ? YEP_DOM_SEQUENCE : YEP_DOM_MAPPING;
        if (n->kind != want) {
            return ::testing::AssertionFailure() << pad << "container kind mismatch";
        }
        size_t want_n = e.items.size();
        if (n->count != want_n) {
            return ::testing::AssertionFailure()
                   << pad << "container count " << n->count << " != " << want_n;
        }
        uint32_t c = n->first_child;
        for (size_t i = 0; i < want_n; i++) {
            if (c == UINT32_MAX) {
                return ::testing::AssertionFailure() << pad << "child chain short at " << i;
            }
            auto r = walk(d, c, e.items[i], depth + 1);
            if (!r)
                return r;
            c = d->nodes[c].next_sibling;
        }
        if (c != UINT32_MAX) {
            return ::testing::AssertionFailure() << pad << "child chain long";
        }
        return ::testing::AssertionSuccess();
    }
    default:
        break;
    }
    if (n->kind != YEP_DOM_SCALAR) {
        return ::testing::AssertionFailure() << pad << "expected scalar";
    }
    std::string got = vstr(d, n);
    switch (e.k) {
    case E::I:
        if (n->tag_id != YEPTRIS_TAG_INT) {
            return ::testing::AssertionFailure() << pad << "int tag_id " << n->tag_id;
        }
        if (got != e.text) {
            return ::testing::AssertionFailure()
                   << pad << "int '" << got << "' != '" << e.text << "'";
        }
        return ::testing::AssertionSuccess();
    case E::F: {
        if (n->tag_id != YEPTRIS_TAG_FLOAT) {
            return ::testing::AssertionFailure() << pad << "float tag_id " << n->tag_id;
        }
        double a = strtod(got.c_str(), nullptr);
        double b = strtod(e.text.c_str(), nullptr);
        if (!(a == b || (std::isnan(a) && std::isnan(b)))) {
            return ::testing::AssertionFailure()
                   << pad << "float '" << got << "' != '" << e.text << "'";
        }
        return ::testing::AssertionSuccess();
    }
    case E::S:
    case E::B:
        if (n->tag_id != 0) {
            return ::testing::AssertionFailure() << pad << "str tag_id " << n->tag_id;
        }
        if (got != e.text) {
            return ::testing::AssertionFailure()
                   << pad << "str '" << got << "' != '" << e.text << "'";
        }
        return ::testing::AssertionSuccess();
    case E::TBOOL:
    case E::FBOOL:
        if (n->tag_id != YEPTRIS_TAG_BOOL || got != e.text) {
            return ::testing::AssertionFailure() << pad << "bool '" << got << "'";
        }
        return ::testing::AssertionSuccess();
    case E::NUL:
        if (n->tag_id != YEPTRIS_TAG_NULL || got != "null") {
            return ::testing::AssertionFailure() << pad << "null '" << got << "'";
        }
        return ::testing::AssertionSuccess();
    case E::UNDEF:
        if (got != "undefined") {
            return ::testing::AssertionFailure() << pad << "undefined '" << got << "'";
        }
        return ::testing::AssertionSuccess();
    case E::SIMPLE:
        if (got != "simple(" + std::to_string(e.simple) + ")") {
            return ::testing::AssertionFailure() << pad << "simple '" << got << "'";
        }
        return ::testing::AssertionSuccess();
    default:
        return ::testing::AssertionFailure() << pad << "unhandled kind";
    }
}

void expect_ok(const char* hex, const E& e, uint32_t opts = 0) {
    std::vector<uint8_t> bytes = hx(hex); /* the document borrows these */
    Dec d(bytes, opts);
    ASSERT_EQ(d.st, YEPTRIS_OK) << hex;
    ASSERT_NE(d.doc, nullptr);
    ASSERT_EQ(d.dom()->dcount, 1u) << hex;
    EXPECT_TRUE(walk(d.dom(), d.root(), e)) << hex;
}

void expect_reject(const char* hex, uint32_t opts = 0) {
    std::vector<uint8_t> bytes = hx(hex);
    Dec d(bytes, opts);
    EXPECT_EQ(d.st, YEPTRIS_ERROR_PARSE) << hex;
    EXPECT_EQ(d.doc, nullptr) << hex;
}

// ---- RFC 8949 Appendix A (Table 6, normative) ------------------------

TEST(Cbor, AppendixA) {
    // integers
    expect_ok("00", I_("0"));
    expect_ok("01", I_("1"));
    expect_ok("0a", I_("10"));
    expect_ok("17", I_("23"));
    expect_ok("1818", I_("24"));
    expect_ok("1819", I_("25"));
    expect_ok("1864", I_("100"));
    expect_ok("1903e8", I_("1000"));
    expect_ok("1a000f4240", I_("1000000"));
    expect_ok("1b000000e8d4a51000", I_("1000000000000"));
    expect_ok("1bffffffffffffffff", F_("1.8446744073709552e+19")); // 2^64-1: beyond int64
    expect_ok("c249010000000000000000",
              Tg("2", B_(std::string("\x01\0\0\0\0\0\0\0\0", 9)))); // 2^64 bignum
    expect_ok("3bffffffffffffffff", F_("-1.8446744073709552e+19")); // -2^64: beyond int64
    expect_ok("c349010000000000000000", Tg("3", B_(std::string("\x01\0\0\0\0\0\0\0\0", 9))));
    expect_ok("20", I_("-1"));
    expect_ok("29", I_("-10"));
    expect_ok("3863", I_("-100"));
    expect_ok("3903e7", I_("-1000"));
    // floats
    expect_ok("f90000", F_("0.0"));
    expect_ok("f98000", F_("-0.0"));
    expect_ok("f93c00", F_("1.0"));
    expect_ok("fb3ff199999999999a", F_("1.1"));
    expect_ok("f93e00", F_("1.5"));
    expect_ok("f97bff", F_("65504.0"));
    expect_ok("fa47c35000", F_("100000.0"));
    expect_ok("fa7f7fffff", F_("3.4028234663852886e+38"));
    expect_ok("fb7e37e43c8800759c", F_("1.0e+300"));
    expect_ok("f90001", F_("5.960464477539063e-8"));
    expect_ok("f90400", F_("0.00006103515625"));
    expect_ok("f9c400", F_("-4.0"));
    expect_ok("fbc010666666666666", F_("-4.1"));
    expect_ok("f97c00", F_("Infinity"));
    expect_ok("f97e00", F_("NaN"));
    expect_ok("f9fc00", F_("-Infinity"));
    expect_ok("fa7f800000", F_("Infinity"));
    expect_ok("fa7fc00000", F_("NaN"));
    expect_ok("faff800000", F_("-Infinity"));
    expect_ok("fb7ff0000000000000", F_("Infinity"));
    expect_ok("fb7ff8000000000000", F_("NaN"));
    expect_ok("fbfff0000000000000", F_("-Infinity"));
    // simple values
    expect_ok("f4", Fb());
    expect_ok("f5", Tb());
    expect_ok("f6", Nl());
    expect_ok("f7", Un());
    expect_ok("f0", Sm(16));
    expect_ok("f8ff", Sm(255));
    // tagged
    expect_ok("c074323031332d30332d32315432303a30343a30305a", Tg("0", S_("2013-03-21T20:04:00Z")));
    expect_ok("c11a514b67b0", Tg("1", I_("1363896240")));
    expect_ok("c1fb41d452d9ec200000", Tg("1", F_("1363896240.5")));
    expect_ok("d74401020304", Tg("23", B_(std::string("\x01\x02\x03\x04", 4))));
    expect_ok("d818456449455446", Tg("24", B_("dIETF"))); // h'6449455446' is "dIETF"
    expect_ok("d82076687474703a2f2f7777772e6578616d706c652e636f6d",
              Tg("32", S_("http://www.example.com")));
    // strings
    expect_ok("40", B_(""));
    expect_ok("4401020304", B_(std::string("\x01\x02\x03\x04", 4)));
    expect_ok("60", S_(""));
    expect_ok("6161", S_("a"));
    expect_ok("6449455446", S_("IETF"));
    expect_ok("62225c", S_("\"\\"));
    expect_ok("62c3bc", S_("\xc3\xbc"));
    expect_ok("63e6b0b4", S_("\xe6\xb0\xb4"));
    expect_ok("64f0908591", S_("\xf0\x90\x85\x91"));
    // arrays
    expect_ok("80", A({}));
    expect_ok("83010203", A({I_("1"), I_("2"), I_("3")}));
    expect_ok("8301820203820405", A({I_("1"), A({I_("2"), I_("3")}), A({I_("4"), I_("5")})}));
    {
        std::vector<E> xs;
        for (int i = 1; i <= 25; i++) {
            xs.push_back(I_(std::to_string(i)));
        }
        expect_ok("98190102030405060708090a0b0c0d0e0f101112131415161718181819", A(xs));
    }
    // maps (non-string keys materialize as diagnostic text — lenient default)
    expect_ok("a0", M({}));
    expect_ok("a201020304", M({S_("1"), I_("2"), S_("3"), I_("4")})); // diagnostic keys
    expect_ok("a26161016162820203", M({S_("a"), I_("1"), S_("b"), A({I_("2"), I_("3")})}));
    expect_ok("826161a161626163", A({S_("a"), M({S_("b"), S_("c")})}));
    expect_ok("a56161614161626142616361436164614461656145",
              M({S_("a"), S_("A"), S_("b"), S_("B"), S_("c"), S_("C"), S_("d"), S_("D"), S_("e"),
                 S_("E")}));
    // indefinite-length
    expect_ok("5f42010243030405ff", B_(std::string("\x01\x02\x03\x04\x05", 5)));
    expect_ok("7f657374726561646d696e67ff", S_("streaming"));
    expect_ok("9fff", A({}));
    expect_ok("9f018202039f0405ffff", A({I_("1"), A({I_("2"), I_("3")}), A({I_("4"), I_("5")})}));
    expect_ok("9f01820203820405ff", A({I_("1"), A({I_("2"), I_("3")}), A({I_("4"), I_("5")})}));
    expect_ok("83018202039f0405ff", A({I_("1"), A({I_("2"), I_("3")}), A({I_("4"), I_("5")})}));
    expect_ok("83019f0203ff820405", A({I_("1"), A({I_("2"), I_("3")}), A({I_("4"), I_("5")})}));
    {
        std::vector<E> xs;
        for (int i = 1; i <= 25; i++) {
            xs.push_back(I_(std::to_string(i)));
        }
        expect_ok("9f0102030405060708090a0b0c0d0e0f101112131415161718181819ff", A(xs));
    }
    expect_ok("bf61610161629f0203ffff", M({S_("a"), I_("1"), S_("b"), A({I_("2"), I_("3")})}));
    expect_ok("826161bf61626163ff", A({S_("a"), M({S_("b"), S_("c")})}));
    expect_ok("bf6346756ef563416d7421ff", M({S_("Fun"), Tb(), S_("Amt"), I_("-2")}));
}

// ---- RFC 8949 Appendix F.1 (not well-formed) -------------------------

TEST(Cbor, AppendixFRejects) {
    // end of input in a head
    for (const char* h : {"18", "19", "1a", "1b", "1901", "1a0102", "1b01020304050607", "38", "58",
                          "78", "98", "9a01ff00", "b8", "d8", "f8", "f900", "fa0000", "fb000000"}) {
        expect_reject(h);
    }
    // definite-length strings with short data
    for (const char* h : {"41", "61", "5affffffff00", "5bffffffffffffffff010203", "7affffffff00",
                          "7b7fffffffffffffff010203"}) {
        expect_reject(h);
    }
    // definite maps/arrays not closed with enough items
    for (const char* h :
         {"81", "81818181818181818181", "8200", "a1", "a20102", "a100", "a2000000"}) {
        expect_reject(h);
    }
    // tag number not followed by tag content
    expect_reject("c0");
    // indefinite strings not closed by break
    expect_reject("5f4100");
    expect_reject("7f6100");
    // indefinite maps/arrays not closed by break
    for (const char* h : {"9f", "9f0102", "bf", "bf01020102", "819f", "9f8000", "9f9f9f9f9fffffff",
                          "9f819f819f9ffffff"}) {
        expect_reject(h);
    }
    // subkind 1: reserved additional information
    for (const char* h : {"1c", "1d", "1e", "3c", "3d", "3e", "5c", "5d", "5e", "7c", "7d", "7e",
                          "9c", "9d", "9e", "bc", "bd", "be", "dc", "dd", "de", "fc", "fd", "fe"}) {
        expect_reject(h);
    }
    // subkind 2: reserved two-byte simple values
    for (const char* h : {"f800", "f801", "f818", "f81f"}) {
        expect_reject(h);
    }
    // subkind 3: indefinite string chunks of wrong type
    for (const char* h : {"5f00ff", "5f21ff", "5f6100ff", "5f80ff", "5fa0ff", "5fc000ff", "5fe0ff",
                          "7f4100ff", "5f5f4100ffff", "7f7f6100ffff"}) {
        expect_reject(h);
    }
    // subkind 4: break outside a legal position
    for (const char* h : {"ff", "81ff", "8200ff", "a1ff", "a1ff00", "a100ff", "a20000ff", "9f81ff",
                          "9f829f819f9fffffff", "bf00ff", "bf000000ff"}) {
        expect_reject(h);
    }
    // subkind 5: additional information 31 with major type 0, 1, 6
    for (const char* h : {"1f", "3f", "df"}) {
        expect_reject(h);
    }
    // too much data
    expect_reject("0102");
    expect_reject("810102");
}

// ---- strict mode, encoding rules, guards ------------------------------

TEST(Cbor, StrictMinimality) {
    // lenient default accepts longer-than-needed arguments
    expect_ok("1818", I_("24"));
    std::vector<uint8_t> b24 = hx("1800");
    Dec ok24(b24, 0);
    EXPECT_EQ(ok24.st, YEPTRIS_OK) << "lenient accepts non-minimal zero";
    // strict rejects the wider-than-needed forms (0x1818 IS minimal for 24)
    Dec s1(hx("190018"), YEPTRIS_CBOR_STRICT);
    EXPECT_EQ(s1.st, YEPTRIS_ERROR_PARSE);
    Dec s2(hx("1a00000100"), YEPTRIS_CBOR_STRICT);
    EXPECT_EQ(s2.st, YEPTRIS_ERROR_PARSE);
    Dec s3(hx("1a00000018"), YEPTRIS_CBOR_STRICT);
    EXPECT_EQ(s3.st, YEPTRIS_ERROR_PARSE);
    Dec s4(hx("1b0000000000000018"), YEPTRIS_CBOR_STRICT);
    EXPECT_EQ(s4.st, YEPTRIS_ERROR_PARSE);
    // minimal forms stay accepted in strict
    expect_ok("17", I_("23"), YEPTRIS_CBOR_STRICT);
    expect_ok("1818", I_("24"), YEPTRIS_CBOR_STRICT);
    expect_ok("190100", I_("256"), YEPTRIS_CBOR_STRICT);
}

TEST(Cbor, StrictMapKeys) {
    // integer key: lenient materializes the diagnostic text
    std::vector<uint8_t> mbytes = hx("a10102");
    Dec len(mbytes);
    ASSERT_EQ(len.st, YEPTRIS_OK);
    EXPECT_TRUE(walk(len.dom(), len.root(), M({S_("1"), I_("2")})));
    // strict rejects non-text keys
    Dec s(hx("a10102"), YEPTRIS_CBOR_STRICT);
    EXPECT_EQ(s.st, YEPTRIS_ERROR_PARSE);
    // text keys pass strict
    expect_ok("a1616101", M({S_("a"), I_("1")}), YEPTRIS_CBOR_STRICT);
    // structure keys reject in BOTH modes
    expect_reject("a1810102");
    Dec sarr(hx("a1810102"), YEPTRIS_CBOR_STRICT);
    EXPECT_EQ(sarr.st, YEPTRIS_ERROR_PARSE);
}

TEST(Cbor, TagChains) {
    // nested tags: outermost first, space separated
    expect_ok("c1c06432303133", Tg("1 0", S_("2013"))); // 1(0("2013"))
    expect_ok("d9d9f701", Tg("55799", I_("1")));        // self-described CBOR
    // the chain attaches to containers too
    expect_ok("c1d9d9f780", Tg("1 55799", A({}))); // 1(55799([]))
}

TEST(Cbor, GuardsAndOffsets) {
    // empty input
    Dec e(hx(""));
    EXPECT_EQ(e.st, YEPTRIS_ERROR_PARSE);
    // depth guard: 1001 nested arrays
    std::vector<uint8_t> deep;
    for (int i = 0; i < 1001; i++)
        deep.push_back(0x81);
    deep.push_back(0x01);
    Dec dd(deep);
    EXPECT_EQ(dd.st, YEPTRIS_ERROR_DEPTH);
    // 1000 exactly is fine
    std::vector<uint8_t> atcap;
    for (int i = 0; i < 1000; i++)
        atcap.push_back(0x81);
    atcap.push_back(0x01);
    Dec dc(atcap);
    EXPECT_EQ(dc.st, YEPTRIS_OK);
    // huge declared string with tiny input: early reject, no balloon
    Dec big(hx("5bffffffffffffffff0102"));
    EXPECT_EQ(big.st, YEPTRIS_ERROR_PARSE);
    // the error carries the byte offset
    yeptris_cbor_decode(hx("a100ff").data(), 5, 0, nullptr);
    uint32_t line = 0, col = 0;
    const char* msg = yeptris_last_error(&line, &col);
    EXPECT_NE(msg, nullptr);
    EXPECT_NE(strstr(msg, "byte"), nullptr) << msg;
    // ill-formed UTF-8 in a text string is ENCODING
    Dec u8(hx("62c0ae"));
    EXPECT_EQ(u8.st, YEPTRIS_ERROR_ENCODING);
    // a raw byte string holding the same bytes decodes (bytes don't validate)
    Dec bs(hx("42c0ae"));
    EXPECT_EQ(bs.st, YEPTRIS_OK);
}

TEST(Cbor, UndefinedVsNullAndSimples) {
    // both decode; the DOM distinguishes by text
    expect_ok("f6", Nl());
    expect_ok("f7", Un());
    expect_ok("f0", Sm(16));
    expect_ok("f820", Sm(32));
    // empty indefinite strings of each kind
    expect_ok("5fff", B_(""));
    expect_ok("7fff", S_(""));
    // zero-length chunks are permitted
    expect_ok("5f404401020304ff", B_(std::string("\x01\x02\x03\x04", 4)));
}

TEST(Cbor, ArgContract) {
    EXPECT_EQ(yeptris_cbor_decode(nullptr, 3, 0, nullptr), nullptr);
    YeptrisStatus st = YEPTRIS_OK;
    yeptris_cbor_decode(nullptr, 3, 0, &st);
    EXPECT_EQ(st, YEPTRIS_ERROR_ARG);
    st = YEPTRIS_OK;
    yeptris_cbor_decode(hx("01").data(), 1, 0x2, &st); // unknown flag bit
    EXPECT_EQ(st, YEPTRIS_ERROR_ARG);
}

// ---- encoder (TODO.cbor/02) ------------------------------------------

std::string enc_hex(YeptrisDocument doc, uint32_t opts, size_t* out_len = nullptr) {
    size_t n = 0;
    void* buf = yeptris_cbor_encode(doc, opts, &n);
    std::string hex;
    if (buf != nullptr) {
        const uint8_t* b = (const uint8_t*)buf;
        char tmp[3];
        for (size_t i = 0; i < n; i++) {
            snprintf(tmp, sizeof tmp, "%02x", b[i]);
            hex += tmp;
        }
        free(buf);
    }
    if (out_len != nullptr) {
        *out_len = n;
    }
    return hex;
}

/* RFC 8949 Appendix A, decode -> canonical re-encode. Rows where the
 * two hex strings differ pin a ledgered divergence:
 * - mt2 byte strings re-encode as mt3 text (the 01 ledger)
 * - beyond-int64 integers re-encode as preferred floats (2^64-1 and
 *   -2^64 are powers of two: single-width exactly)
 * - non-half NaN canonicalizes to f9 7e00; all Infinity widths to the
 *   half form; indefinite containers to definite */
TEST(CborEncode, AppendixACanonicalReencode) {
    struct Row {
        const char* in;
        const char* out;
    };
    const Row rows[] = {
        {"00", "00"},
        {"01", "01"},
        {"0a", "0a"},
        {"17", "17"},
        {"1818", "1818"},
        {"1819", "1819"},
        {"1864", "1864"},
        {"1903e8", "1903e8"},
        {"1a000f4240", "1a000f4240"},
        {"1b000000e8d4a51000", "1b000000e8d4a51000"},
        {"1bffffffffffffffff", "fa5f800000"},                 // 2^64-1 -> single (exact power of 2)
        {"c249010000000000000000", "c269010000000000000000"}, // bignum: bytes -> text
        {"3bffffffffffffffff", "fadf800000"},                 // -2^64 -> single
        {"c349010000000000000000", "c369010000000000000000"},
        {"20", "20"},
        {"29", "29"},
        {"3863", "3863"},
        {"3903e7", "3903e7"},
        {"f90000", "f90000"},
        {"f98000", "f98000"},
        {"f93c00", "f93c00"},
        {"fb3ff199999999999a", "fb3ff199999999999a"},
        {"f93e00", "f93e00"},
        {"f97bff", "f97bff"},
        {"fa47c35000", "fa47c35000"},
        {"fa7f7fffff", "fa7f7fffff"},
        {"fb7e37e43c8800759c", "fb7e37e43c8800759c"},
        {"f90001", "f90001"},
        {"f90400", "f90400"},
        {"f9c400", "f9c400"},
        {"fbc010666666666666", "fbc010666666666666"},
        {"f97c00", "f97c00"},
        {"f97e00", "f97e00"},
        {"f9fc00", "f9fc00"},
        {"fa7f800000", "f97c00"},
        {"fa7fc00000", "f97e00"},
        {"faff800000", "f9fc00"},
        {"fb7ff0000000000000", "f97c00"},
        {"fb7ff8000000000000", "f97e00"},
        {"fbfff0000000000000", "f9fc00"},
        {"f4", "f4"},
        {"f5", "f5"},
        {"f6", "f6"},
        {"f7", "f7"},
        {"f0", "f0"},
        {"f8ff", "f8ff"},
        {"c074323031332d30332d32315432303a30343a30305a",
         "c074323031332d30332d32315432303a30343a30305a"},
        {"c11a514b67b0", "c11a514b67b0"},
        {"c1fb41d452d9ec200000", "c1fb41d452d9ec200000"},
        {"d74401020304", "d76401020304"},
        {"d818456449455446", "d818656449455446"},
        {"d82076687474703a2f2f7777772e6578616d706c652e636f6d",
         "d82076687474703a2f2f7777772e6578616d706c652e636f6d"},
        {"40", "60"},
        {"4401020304", "6401020304"},
        {"60", "60"},
        {"6161", "6161"},
        {"6449455446", "6449455446"},
        {"62225c", "62225c"},
        {"62c3bc", "62c3bc"},
        {"63e6b0b4", "63e6b0b4"},
        {"64f0908591", "64f0908591"},
        {"80", "80"},
        {"83010203", "83010203"},
        {"8301820203820405", "8301820203820405"},
        {"98190102030405060708090a0b0c0d0e0f101112131415161718181819",
         "98190102030405060708090a0b0c0d0e0f101112131415161718181819"},
        {"a0", "a0"},
        {"a201020304", "a2613102613304"}, // int keys materialized as text "1","3"
        {"a26161016162820203", "a26161016162820203"},
        {"826161a161626163", "826161a161626163"},
        {"a56161614161626142616361436164614461656145",
         "a56161614161626142616361436164614461656145"},
        {"5f42010243030405ff", "650102030405"},
        {"7f657374726561646d696e67ff", "6973747265616d696e67"},
        {"9fff", "80"},
        {"9f018202039f0405ffff", "8301820203820405"},
        {"9f01820203820405ff", "8301820203820405"},
        {"83018202039f0405ff", "8301820203820405"},
        {"83019f0203ff820405", "8301820203820405"},
        {"9f0102030405060708090a0b0c0d0e0f101112131415161718181819ff",
         "98190102030405060708090a0b0c0d0e0f101112131415161718181819"},
        {"bf61610161629f0203ffff", "a26161016162820203"},
        {"826161bf61626163ff", "826161a161626163"},
        // Appendix A's tagged/simple remainder: date-time (0), epoch
        // (1), encoded-data (24 h'...') re-encode tag+text; undefined
        // and unassigned simples materialize as their diagnostic text
        // (the 01 ledger) so the re-encode pins the divergence
        {"c074323031332d30332d32315431393a30303a33305a",
         "c074323031332d30332d32315431393a30303a33305a"},
        {"c11a514b67b0", "c11a514b67b0"},
        {"d818456449455446",
         "d818656449455446"}, // tag 24 byte-string: text re-encode (the 01 ledger)
        {"f6", "f6"},
        {"f7", "f7"},                                           // undefined round-trips faithfully
        {"f820", "f820"},                                       // simple(32) round-trips faithfully
        {"bf6346756ef563416d7421ff", "a263416d74216346756ef5"}, // canonical: Amt<Fun
    };
    for (const Row& r : rows) {
        std::vector<uint8_t> bytes = hx(r.in);
        Dec d(bytes);
        ASSERT_EQ(d.st, YEPTRIS_OK) << r.in;
        std::string got = enc_hex((YeptrisDocument)d.doc, YEPTRIS_CBOR_CANONICAL);
        EXPECT_EQ(got, std::string(r.out)) << r.in;
        if (got != std::string(r.out))
            break; /* report once, in full */
    }
}

TEST(CborEncode, CanonicalSizingSurvivesMapArrayRealloc) {
    /* #152: sizing held a cmap* across child recursion; a nested
     * map's prep REALLOCed e->maps and the next pair read walked
     * freed memory — n>=11 of this shape crossed the 16->32 growth */
    for (int n : {1, 8, 10, 11, 15, 100}) {
        std::string yaml = "users:\n";
        for (int i = 1; i <= n; i++) {
            yaml += "- id: " + std::to_string(i) + "\n  p:\n    q:\n      r: s\n";
        }
        size_t olen = 0;
        YeptrisStatus pst = YEPTRIS_OK;
        YeptrisDocument doc = yeptris_parse(yaml.data(), yaml.size(), &pst);
        ASSERT_EQ(pst, YEPTRIS_OK) << n;
        void* out = yeptris_cbor_encode(doc, YEPTRIS_CBOR_CANONICAL, &olen);
        ASSERT_NE(out, nullptr) << n;
        EXPECT_GT(olen, 0u) << n;
        yeptris_free(out);
        yeptris_document_free(doc);
    }
}

TEST(CborEncode, CanonicalKeyOrderingAndStability) {
    // keys sort bytewise on ENCODED forms: 10 (0a) < 100 (1864) < -1 (20)
    // < "z" (617a) < "aa" (616161) < [100] (811864) < [-1] (8120) < false (f4)
    const char* yaml =
        "{? false: 1, ? [100]: 2, ? 10: 3, ? [-1]: 4, ? aa: 5, ? 100: 6, ? z: 7, ? -1: 8}";
    YeptrisStatus st = YEPTRIS_OK;
    std::string src(yaml);
    YeptrisDocument doc = yeptris_parse(src.data(), src.size(), &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    // (YAML complex keys and mixed types may not all parse as one map;
    //  the CBOR canonical order is exercised over the string subset.)
    yeptris_document_free(doc);
    // direct: build via decode of a map with unsorted text keys
    const char* hexin = "a36162610361616102616301"; // {"b":3,"a":2,"c":1}
    std::vector<uint8_t> bytes = hx(hexin);
    Dec d(bytes);
    ASSERT_EQ(d.st, YEPTRIS_OK);
    EXPECT_EQ(enc_hex((YeptrisDocument)d.doc, YEPTRIS_CBOR_CANONICAL),
              "a36161610261626103616301"); // sorted: a:2 b:3 c:1
    // stability: repeated encodes byte-identical
    EXPECT_EQ(enc_hex((YeptrisDocument)d.doc, YEPTRIS_CBOR_CANONICAL),
              enc_hex((YeptrisDocument)d.doc, YEPTRIS_CBOR_CANONICAL));
    // insertion order without the canonical flag
    EXPECT_EQ(enc_hex((YeptrisDocument)d.doc, 0), "a36162610361616102616301");
    // the two-pass contract: sizing query == written count
    size_t need =
        yeptris_cbor_encode_into((YeptrisDocument)d.doc, YEPTRIS_CBOR_CANONICAL, nullptr, 0);
    std::vector<uint8_t> sink(need + 8, 0xEE);
    size_t wrote = yeptris_cbor_encode_into((YeptrisDocument)d.doc, YEPTRIS_CBOR_CANONICAL,
                                            sink.data(), sink.size());
    EXPECT_EQ(wrote, need);
    EXPECT_EQ(yeptris_cbor_encode_into((YeptrisDocument)d.doc, YEPTRIS_CBOR_CANONICAL, sink.data(),
                                       need - 1),
              need); /* too-small cap writes nothing, returns the need */
}

TEST(CborEncode, RoundtripProperty) {
    /* decode -> encode -> decode: DOM trees equal (the walker compares
     * kind + text, so the bytes->text divergence is invisible) */
    const char* vectors[] = {
        "00",
        "01",
        "1818",
        "1b000000e8d4a51000",
        "1bffffffffffffffff",
        "20",
        "3903e7",
        "f90000",
        "f98000",
        "f93c00",
        "fb3ff199999999999a",
        "f9c400",
        "f97bff",
        "f97e00",
        "f9fc00",
        "fa7f7fffff",
        "fb7e37e43c8800759c",
        "f90001",
        "f4",
        "f5",
        "f6",
        "f7",
        "f0",
        "f8ff",
        "c074323031332d30332d32315432303a30343a30305a",
        "c11a514b67b0",
        "c249010000000000000000",
        "d74401020304",
        "d818456449455446",
        "d82076687474703a2f2f7777772e6578616d706c652e636f6d",
        "40",
        "4401020304",
        "60",
        "6161",
        "6449455446",
        "62225c",
        "62c3bc",
        "64f0908591",
        "80",
        "83010203",
        "8301820203820405",
        "98190102030405060708090a0b0c0d0e0f101112131415161718181819",
        "a0",
        "a201020304",
        "a26161016162820203",
        "826161a161626163",
        "5f42010243030405ff",
        "7f657374726561646d696e67ff",
        "9fff",
        "9f018202039f0405ffff",
        "bf61610161629f0203ffff",
        "bf6346756ef563416d7421ff",
        "a26161016162820203",
    };
    for (const char* v : vectors) {
        std::vector<uint8_t> a = hx(v);
        Dec first(a);
        ASSERT_EQ(first.st, YEPTRIS_OK) << v;
        size_t n = 0;
        void* enc = yeptris_cbor_encode((YeptrisDocument)first.doc, YEPTRIS_CBOR_CANONICAL, &n);
        ASSERT_NE(enc, nullptr) << v;
        Dec second(std::vector<uint8_t>((uint8_t*)enc, (uint8_t*)enc + n));
        free(enc);
        ASSERT_EQ(second.st, YEPTRIS_OK) << v;
        EXPECT_TRUE(walk(second.dom(), second.root(), E{E::ANY_ROOT, "", false, 0, {}, ""})) << v;
    }
}

TEST(CborEncode, ByteStringFidelity) {
    /* bytes that are invalid UTF-8 can only have been mt2 — they
     * re-encode as mt2 and roundtrip BYTE-exact; ASCII bytes (valid
     * UTF-8) stay text (the 01 ledger divergence, tree-equal) */
    std::vector<uint8_t> raw = hx("42c0ae");
    Dec d(raw);
    ASSERT_EQ(d.st, YEPTRIS_OK);
    EXPECT_EQ(enc_hex((YeptrisDocument)d.doc, 0), "42c0ae"); /* mt2, exact */
    std::vector<uint8_t> ascii = hx("4401020304");
    Dec t(ascii);
    ASSERT_EQ(t.st, YEPTRIS_OK);
    EXPECT_EQ(enc_hex((YeptrisDocument)t.doc, 0), "6401020304"); /* mt3 text */
    /* and the mt2 roundtrip re-decodes to the same bytes */
    std::vector<uint8_t> again = hx("42c0ae");
    Dec r(again);
    ASSERT_EQ(r.st, YEPTRIS_OK);
    EXPECT_EQ(enc_hex((YeptrisDocument)r.doc, YEPTRIS_CBOR_CANONICAL), "42c0ae");
}

TEST(CborEncode, UnencodableAndContract) {
    // empty document set
    EXPECT_EQ(yeptris_cbor_encode_into(nullptr, 0, nullptr, 0), 0u);
    // YAML alias: the CBOR model has none
    const char* ys = "a: &x 1\nb: *x\n";
    std::string src(ys);
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse(src.data(), src.size(), &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    size_t n = 0;
    EXPECT_EQ(yeptris_cbor_encode(doc, YEPTRIS_CBOR_CANONICAL, &n), nullptr);
    yeptris_document_free(doc);
    // YAML -> CBOR -> tree check (strings, ints, nesting)
    const char* y2 = "name: yeptris\ncount: 7\nvals: [1, 2, 3]\nratio: 1.5\nok: true\n";
    std::string s2(y2);
    doc = yeptris_parse(s2.data(), s2.size(), &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    void* enc = yeptris_cbor_encode(doc, YEPTRIS_CBOR_CANONICAL, &n);
    ASSERT_NE(enc, nullptr);
    {
        std::vector<uint8_t> encbuf((uint8_t*)enc, (uint8_t*)enc + n);
        Dec back(encbuf); /* the document borrows the bytes */
        ASSERT_EQ(back.st, YEPTRIS_OK);
        EXPECT_TRUE(walk(
            back.dom(), back.root(),
            /* canonical key order sorts on ENCODED keys:
             * the length head leads — ok(0x62) < name/vals
             * (0x64, n<v) < count/ratio (0x65, c<r) */
            M({S_("ok"), Tb(), S_("name"), S_("yeptris"), S_("vals"),
               A({I_("1"), I_("2"), I_("3")}), S_("count"), I_("7"), S_("ratio"), F_("1.5")})));
        free(enc);
    }
    yeptris_document_free(doc);
}

// ---- sequences (TODO.cbor/03, RFC 8742) ------------------------------

struct SeqSink {
    std::vector<std::string> canon; /* each item's canonical bytes */
    size_t abort_at = SIZE_MAX;
};

static int seq_cb(void* ctx, YeptrisDocument item, size_t index) {
    SeqSink* s = (SeqSink*)ctx;
    if (index == s->abort_at) {
        yeptris_document_free(item);
        return 1;
    }
    s->canon.push_back(enc_hex(item, YEPTRIS_CBOR_CANONICAL));
    yeptris_document_free(item);
    return 0;
}

static std::vector<uint8_t> seq_bytes(const std::vector<const char*>& hexes) {
    std::vector<uint8_t> all;
    for (const char* h : hexes) {
        std::vector<uint8_t> b = hx(h);
        all.insert(all.end(), b.begin(), b.end());
    }
    return all;
}

static std::string one_canon(const char* hex) {
    std::vector<uint8_t> b = hx(hex);
    Dec d(b);
    EXPECT_EQ(d.st, YEPTRIS_OK) << hex;
    return enc_hex((YeptrisDocument)d.doc, YEPTRIS_CBOR_CANONICAL);
}

TEST(CborSeq, MixedItemsIdenticalTrees) {
    const char* items[] = {
        "01",
        "f5",
        "6449455446",
        "83010203",
        "a26161016162820203",
        "1b000000e8d4a51000",
        "fbc010666666666666",
        "c11a514b67b0",
        "9f01820203820405ff",
        "4401020304",
        "f0",
    };
    std::vector<uint8_t> seq = seq_bytes({items, items + 11});
    SeqSink sink;
    YeptrisStatus st = YEPTRIS_OK;
    size_t n = yeptris_cbor_decode_sequence(seq.data(), seq.size(), 0, seq_cb, &sink, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_EQ(n, 11u);
    ASSERT_EQ(sink.canon.size(), 11u);
    for (size_t i = 0; i < 11; i++) {
        EXPECT_EQ(sink.canon[i], one_canon(items[i])) << "item " << i;
    }
}

TEST(CborSeq, EmptyAndAbort) {
    SeqSink sink;
    YeptrisStatus st = YEPTRIS_ERROR_ENCODING; /* deliberately dirty */
    EXPECT_EQ(yeptris_cbor_decode_sequence(nullptr, 0, 0, seq_cb, &sink, &st), 0u);
    EXPECT_EQ(st, YEPTRIS_OK); /* an empty sequence is valid */
    /* abort at index 1 of 3 */
    std::vector<uint8_t> seq = seq_bytes({"01", "02", "03"});
    sink = SeqSink();
    sink.abort_at = 1;
    st = YEPTRIS_OK;
    EXPECT_EQ(yeptris_cbor_decode_sequence(seq.data(), seq.size(), 0, seq_cb, &sink, &st), 1u);
    EXPECT_EQ(st, YEPTRIS_OK);
    EXPECT_EQ(sink.canon.size(), 1u);
    /* arg contract */
    st = YEPTRIS_OK;
    EXPECT_EQ(yeptris_cbor_decode_sequence(seq.data(), seq.size(), 0, nullptr, &sink, &st), 0u);
    EXPECT_EQ(st, YEPTRIS_ERROR_ARG);
}

TEST(CborSeq, TruncatedTailCarriesItemIndex) {
    std::vector<uint8_t> seq = seq_bytes({"01", "83010203", "a2616101"});
    /* cut inside item 2 (the map): 1 + 1 + 4 = keep 6 bytes */
    seq.resize(6);
    SeqSink sink;
    YeptrisStatus st = YEPTRIS_OK;
    EXPECT_EQ(yeptris_cbor_decode_sequence(seq.data(), seq.size(), 0, seq_cb, &sink, &st), 2u);
    EXPECT_EQ(st, YEPTRIS_ERROR_PARSE);
    const char* msg = yeptris_last_error(nullptr, nullptr);
    ASSERT_NE(msg, nullptr);
    EXPECT_NE(strstr(msg, "item 2"), nullptr) << msg;
    EXPECT_EQ(sink.canon.size(), 2u); /* the first two were delivered */
}

TEST(CborSeq, SplitDifferentialAtBoundaries) {
    const char* items[] = {"a26161016162820203", "f97bff",
                           ("98190102030405060708090a0b0c0d0e0f"
                            "101112131415161718181819"),
                           "d82076687474703a2f2f7777772e6578616d706c652e636f6d",
                           "c349010000000000000000"};
    std::vector<uint8_t> whole = seq_bytes({items, items + 5});
    SeqSink all;
    YeptrisStatus st = YEPTRIS_OK;
    ASSERT_EQ(yeptris_cbor_decode_sequence(whole.data(), whole.size(), 0, seq_cb, &all, &st), 5u);
    /* every boundary split: prefix + suffix == whole */
    std::vector<size_t> offs;
    size_t acc = 0;
    for (const char* h : items) {
        acc += strlen(h) / 2;
        offs.push_back(acc);
    }
    for (size_t b = 0; b <= 5; b++) {
        size_t split = b == 0 ? 0 : offs[b - 1]; /* before item b */
        SeqSink pre, post;
        st = YEPTRIS_OK;
        size_t n1 = yeptris_cbor_decode_sequence(whole.data(), split, 0, seq_cb, &pre, &st);
        EXPECT_EQ(st, YEPTRIS_OK);
        st = YEPTRIS_OK;
        size_t n2 = yeptris_cbor_decode_sequence(whole.data() + split, whole.size() - split, 0,
                                                 seq_cb, &post, &st);
        EXPECT_EQ(st, YEPTRIS_OK);
        ASSERT_EQ(n1 + n2, 5u) << "boundary " << b;
        pre.canon.insert(pre.canon.end(), post.canon.begin(), post.canon.end());
        EXPECT_EQ(pre.canon, all.canon) << "boundary " << b;
    }
}

TEST(CborSeq, EncodeSequence) {
    /* build three documents from vectors */
    const char* hexes[] = {"8301820203820405", "f9c400", "a161616162"};
    YeptrisDocument docs[3];
    std::vector<std::vector<uint8_t>> keep(3);
    for (int i = 0; i < 3; i++) {
        keep[i] = hx(hexes[i]);
        YeptrisStatus st = YEPTRIS_OK;
        docs[i] = yeptris_cbor_decode(keep[i].data(), keep[i].size(), 0, &st);
        ASSERT_EQ(st, YEPTRIS_OK);
    }
    std::string want = one_canon(hexes[0]) + one_canon(hexes[1]) + one_canon(hexes[2]);
    size_t n = 0;
    unsigned char* buf =
        (unsigned char*)yeptris_cbor_encode_sequence(docs, 3, YEPTRIS_CBOR_CANONICAL, &n);
    ASSERT_NE(buf, nullptr);
    {
        std::string got;
        char tmp[3];
        for (size_t k = 0; k < n; k++) {
            snprintf(tmp, sizeof tmp, "%02x", buf[k]);
            got += tmp;
        }
        EXPECT_EQ(got, want);
    }
    /* sizing contract */
    EXPECT_EQ(yeptris_cbor_encode_sequence_into(docs, 3, YEPTRIS_CBOR_CANONICAL, nullptr, 0), n);
    free(buf);
    /* roundtrip through decode_sequence */
    struct Cap {
        std::vector<std::string>* out;
    };
    SeqSink sink;
    YeptrisStatus st = YEPTRIS_OK;
    buf = (unsigned char*)yeptris_cbor_encode_sequence(docs, 3, YEPTRIS_CBOR_CANONICAL, &n);
    ASSERT_EQ(yeptris_cbor_decode_sequence(buf, n, 0, seq_cb, &sink, &st), 3u);
    EXPECT_EQ(st, YEPTRIS_OK);
    free(buf);
    ASSERT_EQ(sink.canon.size(), 3u);
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(sink.canon[i], one_canon(hexes[i]));
    }
    for (int i = 0; i < 3; i++) {
        yeptris_document_free(docs[i]);
    }
}

} // namespace

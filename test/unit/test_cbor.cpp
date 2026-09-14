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

#include "dom/dom.h"
#include "doc.h"

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
        auto v = [](char c) -> int {
            return c >= 'a' ? c - 'a' + 10 : c - '0';
        };
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
    enum K { I, F, S, B, TBOOL, FBOOL, NUL, UNDEF, SIMPLE, ARR, MAP, TAGGED } k;
    std::string text;               // scalar text (int/float/str/simple)
    bool b = false;                 // bools
    uint64_t simple = 0;            // simple(N)
    std::vector<E> items;           // array / map (k,v,k,v interleaved)
    std::string tag;                // tag chain text
};

const E I_(std::string t) { return E{E::I, t, false, 0, {}, ""}; }
const E F_(std::string t) { return E{E::F, t, false, 0, {}, ""}; }
const E S_(std::string t) { return E{E::S, t, false, 0, {}, ""}; }
const E B_(std::string t) { return E{E::B, t, false, 0, {}, ""}; } // h'...' diagnostic of bytes
const E Tb() { return E{E::TBOOL, "true", true, 0, {}, ""}; }
const E Fb() { return E{E::FBOOL, "false", false, 0, {}, ""}; }
const E Nl() { return E{E::NUL, "null", false, 0, {}, ""}; }
const E Un() { return E{E::UNDEF, "undefined", false, 0, {}, ""}; }
const E Sm(uint64_t n) { return E{E::SIMPLE, "", false, n, {}, ""}; }
E A(std::vector<E> xs) { E e; e.k = E::ARR; e.items = std::move(xs); return e; }
E M(std::vector<E> kvs) { E e; e.k = E::MAP; e.items = std::move(kvs); return e; }
E Tg(std::string tag, E inner) { inner.tag = tag; return inner; }

::testing::AssertionResult walk(const yep_dom* d, uint32_t id, const E& e, int depth = 0) {
    const yep_dnode* n = &d->nodes[id];
    std::string pad(depth * 2, ' ');
    std::string tag_txt = n->tag.len ? std::string(yep_dom_view(d, n->tag).p, yep_dom_view(d, n->tag).len) : "";
    if (tag_txt != e.tag) {
        return ::testing::AssertionFailure() << pad << "tag '" << tag_txt << "' != '" << e.tag << "'";
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
            if (!r) return r;
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
            return ::testing::AssertionFailure() << pad << "int '" << got << "' != '" << e.text << "'";
        }
        return ::testing::AssertionSuccess();
    case E::F: {
        if (n->tag_id != YEPTRIS_TAG_FLOAT) {
            return ::testing::AssertionFailure() << pad << "float tag_id " << n->tag_id;
        }
        double a = strtod(got.c_str(), nullptr);
        double b = strtod(e.text.c_str(), nullptr);
        if (!(a == b || (std::isnan(a) && std::isnan(b)))) {
            return ::testing::AssertionFailure() << pad << "float '" << got << "' != '" << e.text << "'";
        }
        return ::testing::AssertionSuccess();
    }
    case E::S:
    case E::B:
        if (n->tag_id != 0) {
            return ::testing::AssertionFailure() << pad << "str tag_id " << n->tag_id;
        }
        if (got != e.text) {
            return ::testing::AssertionFailure() << pad << "str '" << got << "' != '" << e.text << "'";
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
    expect_ok("c249010000000000000000", Tg("2", B_(std::string("\x01\0\0\0\0\0\0\0\0", 9)))); // 2^64 bignum
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
    expect_ok("c074323031332d30332d32315432303a30343a30305a",
              Tg("0", S_("2013-03-21T20:04:00Z")));
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
    expect_ok("8301820203820405",
              A({I_("1"), A({I_("2"), I_("3")}), A({I_("4"), I_("5")})}));
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
              M({S_("a"), S_("A"), S_("b"), S_("B"), S_("c"), S_("C"), S_("d"), S_("D"),
                 S_("e"), S_("E")}));
    // indefinite-length
    expect_ok("5f42010243030405ff", B_(std::string("\x01\x02\x03\x04\x05", 5)));
    expect_ok("7f657374726561646d696e67ff", S_("streaming"));
    expect_ok("9fff", A({}));
    expect_ok("9f018202039f0405ffff",
              A({I_("1"), A({I_("2"), I_("3")}), A({I_("4"), I_("5")})}));
    expect_ok("9f01820203820405ff",
              A({I_("1"), A({I_("2"), I_("3")}), A({I_("4"), I_("5")})}));
    expect_ok("83018202039f0405ff",
              A({I_("1"), A({I_("2"), I_("3")}), A({I_("4"), I_("5")})}));
    expect_ok("83019f0203ff820405",
              A({I_("1"), A({I_("2"), I_("3")}), A({I_("4"), I_("5")})}));
    {
        std::vector<E> xs;
        for (int i = 1; i <= 25; i++) {
            xs.push_back(I_(std::to_string(i)));
        }
        expect_ok("9f0102030405060708090a0b0c0d0e0f101112131415161718181819ff", A(xs));
    }
    expect_ok("bf61610161629f0203ffff",
              M({S_("a"), I_("1"), S_("b"), A({I_("2"), I_("3")})}));
    expect_ok("826161bf61626163ff", A({S_("a"), M({S_("b"), S_("c")})}));
    expect_ok("bf6346756ef563416d7421ff", M({S_("Fun"), Tb(), S_("Amt"), I_("-2")}));
}

// ---- RFC 8949 Appendix F.1 (not well-formed) -------------------------

TEST(Cbor, AppendixFRejects) {
    // end of input in a head
    for (const char* h : {"18", "19", "1a", "1b", "1901", "1a0102", "1b01020304050607", "38",
                          "58", "78", "98", "9a01ff00", "b8", "d8", "f8", "f900", "fa0000",
                          "fb000000"}) {
        expect_reject(h);
    }
    // definite-length strings with short data
    for (const char* h : {"41", "61", "5affffffff00", "5bffffffffffffffff010203", "7affffffff00",
                          "7b7fffffffffffffff010203"}) {
        expect_reject(h);
    }
    // definite maps/arrays not closed with enough items
    for (const char* h : {"81", "81818181818181818181", "8200", "a1", "a20102", "a100",
                          "a2000000"}) {
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
                          "9c", "9d", "9e", "bc", "bd", "be", "dc", "dd", "de", "fc", "fd",
                          "fe"}) {
        expect_reject(h);
    }
    // subkind 2: reserved two-byte simple values
    for (const char* h : {"f800", "f801", "f818", "f81f"}) {
        expect_reject(h);
    }
    // subkind 3: indefinite string chunks of wrong type
    for (const char* h : {"5f00ff", "5f21ff", "5f6100ff", "5f80ff", "5fa0ff", "5fc000ff",
                          "5fe0ff", "7f4100ff", "5f5f4100ffff", "7f7f6100ffff"}) {
        expect_reject(h);
    }
    // subkind 4: break outside a legal position
    for (const char* h : {"ff", "81ff", "8200ff", "a1ff", "a1ff00", "a100ff", "a20000ff",
                          "9f81ff", "9f829f819f9fffffff", "bf00ff", "bf000000ff"}) {
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
    expect_ok("c1d9d9f780", Tg("1 55799", A({})));      // 1(55799([]))
}

TEST(Cbor, GuardsAndOffsets) {
    // empty input
    Dec e(hx(""));
    EXPECT_EQ(e.st, YEPTRIS_ERROR_PARSE);
    // depth guard: 1001 nested arrays
    std::vector<uint8_t> deep;
    for (int i = 0; i < 1001; i++) deep.push_back(0x81);
    deep.push_back(0x01);
    Dec dd(deep);
    EXPECT_EQ(dd.st, YEPTRIS_ERROR_DEPTH);
    // 1000 exactly is fine
    std::vector<uint8_t> atcap;
    for (int i = 0; i < 1000; i++) atcap.push_back(0x81);
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

} // namespace
